#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include <stdlib.h>

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

  boost::regex rgx = boost::regex(LOGENTRY_REGEX);
  boost::smatch result;
};

using LogEntryRef = LogEntry<std::string_view>;
using LogEntryVal = LogEntry<std::string>;

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
      } while(!std::filesystem::exists(file) && !atEnd());

      if(!std::filesystem::exists(file)) {
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

class Node
{
  public:
  Node() = default;
  ~Node() = default;

  template<typename LogEntry>
  bool operator()(const LogEntry& entry)
  {
    return true;
  }

  private:
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
    *node = &m_nodes[id];
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
    std::cerr << "New host mapping: " << name << " -> " << id << endl;
    return true;
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
}
