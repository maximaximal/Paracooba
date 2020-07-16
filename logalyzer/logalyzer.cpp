#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#if __has_include(<charconv>)
#define USE_CHARCONV
#include <charconv>
#endif

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

namespace po = boost::program_options;
using std::cerr;
using std::cout;
using std::endl;

//  This is the unencoded regex: ^\[([a-zA-Z0-9_.:\ -]+)\] \[([a-zA-Z.;0-9]+)\]
//  \[[\^\[0-9;m]*([A-Z]+)[\^\[0-9m]*\] \[(\w+)\] \[(.*)\] (.*)
// Developed using

#define LOGENTRY_REGEX \
  R"(\[([a-zA-Z0-9_.:\ -]+)\] \[([a-zA-Z.;0-9]+)\] \[[\^\[0-9;m]*([A-Z]+)[\^\[0-9m]*\] \[(\w+)\] \[(.*)\] (.*))"

#define CONNECTION_SENDING_REGEX \
  R"(Now sending message of type ([a-zA-Z]+) with size .+ \(=([0-9]+) BYTES\))"

// Copied over from util.cpp so there is no link required.
std::string
BytePrettyPrint(size_t bytes)
{
  auto base = (double)std::log(bytes) / (double)std::log(1024);
  const char* suffixArr[] = { "B", "kiB", "MiB", "GiB", "TiB", "PiB" };
  return std::to_string(
           (size_t)std::round(std::pow(1024, base - std::floor(base)))) +
         suffixArr[(size_t)std::floor(base)] + " (=" + std::to_string(bytes) +
         " BYTES)";
}

template<typename StringContainer = std::string_view>
class LogEntry
{
  public:
  LogEntry() {}
  ~LogEntry() {}

  /// Cosntruct from string_view based entry.
  LogEntry(const LogEntry<std::string_view>& o)
    : datetime(o.datetime)
    , host(o.host)
    , severity(o.severity)
    , thread(o.thread)
    , logger(o.logger)
    , msg(o.msg)
  {}

  bool parseLogLine(const std::string& line)
  {
    if(boost::regex_search(line, result, rgx)) {
      datetime = StringContainer(result[1].first.base(), result[1].length());
      host = StringContainer(result[2].first.base(), result[2].length());
      severity = StringContainer(result[3].first.base(), result[3].length());
      thread = StringContainer(result[4].first.base(), result[4].length());
      logger = StringContainer(result[5].first.base(), result[5].length());
      msg = StringContainer(result[6].first.base(), result[6].length());

      size_t ctxStartPos = logger.find('<');
      if(ctxStartPos != std::string::npos) {
        ctx = StringContainer(logger.begin() + ctxStartPos + 1,
                              logger.size() - ctxStartPos - 2);
        logger = StringContainer(logger.begin(), ctxStartPos);
        hasCtx = true;
      } else {
        hasCtx = false;
      }
      return true;
    }
    return false;
  }

  StringContainer datetime;
  StringContainer host;
  StringContainer severity;
  StringContainer thread;
  StringContainer logger;
  StringContainer msg;

  StringContainer ctx;
  bool hasCtx;

  boost::regex rgx = boost::regex(LOGENTRY_REGEX);
  boost::smatch result;
};

using LogEntryRef = LogEntry<std::string_view>;
using LogEntryVal = LogEntry<std::string>;

template<typename T>
bool
toNumber(const std::string_view& view, T& number)
{
#if USE_CHARCONV
  if(auto [p, ec] =
       std::from_chars(view.data(), view.data() + view.size(), number);
     ec == std::errc()) {
    return true;
  }
  return false;
#else
  std::string str(view);
  number = std::atoi(str.c_str());
  return true;
#endif
}

bool
extractBytes(std::string_view view, uint64_t& bytes)
{
  auto pos = view.find("(=");
  if(pos == std::string::npos)
    return false;

  pos += 2;
  auto posAfter = view.find(' ', pos);
  if(posAfter == std::string::npos)
    return false;

  return toNumber(view.substr(pos, posAfter - pos), bytes);
}

class LogLineProducer
{
  public:
  explicit LogLineProducer(std::vector<std::string> files)
    : m_files(files)
    , m_filesIt(m_files.begin())
  {}
  ~LogLineProducer() {}

  bool nextLine(std::string& line)
  {
    if(useSTDIN()) {
      return static_cast<bool>(std::getline(std::cin, line));
    } else {
      while(m_currentIfstream.is_open() || !atEnd()) {
        if(!updateIfstream())
          return false;

        if(static_cast<bool>(std::getline(m_currentIfstream, line))) {
          return true;
        } else {
          m_currentIfstream.close();
        }
      }
    }
    return false;
  }

  private:
  bool useSTDIN() const { return m_files.size() == 0; }
  bool atEnd() const { return m_filesIt == m_files.end(); }
  bool updateIfstream()
  {
    if(m_currentIfstream.is_open())
      return true;

    if(!atEnd()) {
      std::string file = "";
      do {
        file = *m_filesIt++;
      } while(!boost::filesystem::exists(file) && !atEnd());

      if(!boost::filesystem::exists(file)) {
        return false;
      }

      m_currentIfstream.open(file);
      return true;
    }
    return false;
  }

  std::vector<std::string> m_files;
  std::vector<std::string>::const_iterator m_filesIt;
  std::ifstream m_currentIfstream;
};

class Connection
{
  public:
  explicit Connection(uint64_t id1, uint64_t id2)
    : m_id1(id1)
    , m_id2(id2)
  {
    assert(id1 != 0);
    assert(id2 != 0);
  }
  ~Connection() = default;

  bool operator()(const LogEntryRef& entry)
  {
    if(boost::regex_search(std::string(entry.msg), m_result, m_sendRegex)) {
      std::string type = m_result[1];
      size_t bytes = std::strtoul(m_result[2].str().c_str(), NULL, 0);

      m_bytesTransferred += bytes;
    }
    return true;
  }

  void print()
  {
    std::cout << "Connection " << m_id1 << " <-> " << m_id2
              << " Statistics: " << endl;
    std::cout << "  Data Transferred: " << BytePrettyPrint(m_bytesTransferred)
              << endl;
  }

  private:
  uint64_t m_id1, m_id2;

  uint64_t m_bytesTransferred;

  static boost::regex m_sendRegex;
  boost::smatch m_result;
};
boost::regex Connection::m_sendRegex = boost::regex(CONNECTION_SENDING_REGEX);

class ConnectionsStats
{
  public:
  ConnectionsStats() = default;
  ~ConnectionsStats() = default;

  using IDPair = std::pair<uint64_t, uint64_t>;

  bool operator()(uint64_t currentId, const LogEntryRef& entry)
  {
    assert(entry.logger == "Connection");

    uint64_t remoteId = 0;
    if(!getRemoteId(entry, remoteId))
      return false;

    // This happens on connection init.
    if(remoteId == 0)
      return false;

    Connection& conn = getConnection(currentId, remoteId);
    if(!conn(entry))
      return false;

    return true;
  }

  bool getRemoteId(const LogEntryRef& entry, uint64_t& id)
  {
    if(!entry.hasCtx)
      return false;

    size_t idStart = 6;
    size_t idEnd = entry.ctx.find('\'', idStart + 1);

    if(idEnd == std::string::npos) {
      return false;
    }

    auto numberStr = entry.ctx.substr(idStart, idEnd - idStart);

    return toNumber(numberStr, id);
  }

  Connection& getConnection(uint64_t id1, uint64_t id2)
  {
    IDPair pair(std::min(id1, id2), std::max(id1, id2));
    auto it = m_links.find(pair);
    if(it == m_links.end()) {
      return m_links.try_emplace(pair, pair.first, pair.second).first->second;
    }
    return it->second;
  }

  void print()
  {
    for(auto& e : m_links) {
      e.second.print();
    }
  }

  private:
  std::map<IDPair, Connection> m_links;
};

ConnectionsStats connections;

class Node
{
  public:
  explicit Node(uint64_t id)
    : m_id(id)
  {}
  ~Node() = default;

  bool operator()(const LogEntryRef& entry)
  {
    if(entry.logger == "Connection") {
      if(!connections(m_id, entry)) {
        return false;
      }
    }

    auto it = m_severities.find(entry.severity);
    if(it == m_severities.end()) {
      m_severities.insert({ std::string(entry.severity), 1 });
    } else {
      ++it->second;
    }

    return true;
  }

  void print()
  {
    cout << "Statistics for node " << m_host << " (" << m_id << ")" << endl;
    cout << "  Log Messages per Severity:" << endl;
    for(auto& e : m_severities) {
      cout << "    " << e.first << ": " << e.second << endl;
    }
  }

  void setHost(const std::string host) { m_host = host; }

  private:
  std::string m_host;
  std::map<std::string, size_t, std::less<>> m_severities;
  uint64_t m_id;
};

class NodeStats
{
  public:
  NodeStats() = default;
  ~NodeStats() = default;

  template<typename LogEntry>
  bool operator()(const LogEntry& entry)
  {
    // Initialization logic
    if(entry.logger == "main") {
      if(boost::regex_search(std::string(entry.msg), m_result, m_nameIdRgx)) {
        map(std::strtoul(m_result[1].str().c_str(), NULL, 0),
            std::string(entry.host));
      }
    }

    // Handling of node specific stuff.
    Node* n;
    if(!getFromMap(entry.host, &n))
      return false;

    if(!(*n)(entry))
      return false;

    return true;
  }

  bool getFromMap(const std::string_view& view, Node** node)
  {
    auto it = m_nameIdMap.find(view);
    if(it == m_nameIdMap.end())
      return false;
    return getFromMap(it->second, node);
  }
  bool getFromMap(const std::string& name, Node** node)
  {
    auto it = m_nameIdMap.find(name);
    if(it == m_nameIdMap.end())
      return false;
    return getFromMap(it->second, node);
  }
  bool getFromMap(uint64_t id, Node** node)
  {
    auto it = m_nodes.find(id);
    if(it == m_nodes.end()) {
      *node = &m_nodes.emplace(id, id).first->second;
    } else {
      *node = &it->second;
    }
    return true;
  }
  bool map(uint64_t id, const std::string& name)
  {
    auto it = m_nameIdMap.find(name);
    if(it != m_nameIdMap.end()) {
      if(it->second != id) {
        cerr << "ID Conflict! Previously, " << name << " was ID " << it->second
             << ". Now, " << id << "!";
        return false;
      }
      return true;
    }
    m_nameIdMap.insert({ name, id });
    Node* n;
    getFromMap(id, &n);
    n->setHost(name);
    return true;
  }

  void print()
  {
    for(auto& n : m_nodes) {
      n.second.print();
    }
  }

  private:
  std::map<uint64_t, Node> m_nodes;
  std::map<std::string, uint64_t, std::less<>> m_nameIdMap;

  boost::regex m_nameIdRgx = boost::regex(R"(= ([0-9]*))");
  boost::smatch m_result;
};

int
main(int argc, char* argv[])
{
  po::options_description desc("Allowed options");
  po::positional_options_description posDesc;
  // clang-format off
  desc.add_options()
    ("help", "produce help message")
    ("file", po::value<std::vector<std::string>>()
         ->multitoken()->zero_tokens()->composing(), "produce help message")
  ;
  posDesc.add("file", -1);
  // clang-format on
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(desc)
              .positional(posDesc)
              .allow_unregistered()
              .run(),
            vm);
  po::notify(vm);

  if(vm.count("help")) {
    cout << "This tool helps analyzing logfiles of the parac SAT solver."
         << endl;
    cout << "That can be useful if paths get lost during solving or in" << endl;
    cout << "other network related issues." << endl;
    cout << "Input files as parameters, default is STDIN." << endl;
    cout << desc << endl;
    return 1;
  }

  std::vector<std::string> files;

  if(vm.count("file")) {
    files = vm["file"].as<std::vector<std::string>>();
  }

  LogLineProducer producer(files);

  LogEntryRef entry;
  NodeStats stats;

  std::string line;
  while(producer.nextLine(line)) {
    if(entry.parseLogLine(line)) {
      stats(entry);
    }
  }

  stats.print();
  connections.print();
}
