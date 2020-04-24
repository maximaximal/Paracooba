#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

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
  R"(^\[([a-zA-Z0-9_.:\ -]+)\] \[([a-zA-Z.;0-9]+)\] " \
    "\[[\^\[0-9;m]*([A-Z]+)[\^\[0-9m]*\] \[(\w+)\] \[(.*)\] (.*))"

template<typename StringContainer = std::string_view>
class LogEntry
{
  public:
  LogEntry()
    : rgx(LOGENTRY_REGEX)
  {}
  ~LogEntry() {}

  bool parseLogLine(const std::string& line)
  {
    auto rit = boost::cregex_token_iterator(
      line.data(), line.data() + line.length(), rgx, 6);
    auto rend = boost::cregex_token_iterator();

    while(rit != rend) {
      cout << "Matched: " << *rit << endl;
      ++rit;
    }

    return true;
  }

  StringContainer datetime;
  StringContainer host;
  StringContainer severity;
  StringContainer thread;
  StringContainer logger;
  StringContainer msg;

  boost::regex rgx;
};

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

  LogEntry entry;
  std::string line;
  while(producer.nextLine(line)) {
    if(entry.parseLogLine(line)) {
      cout << entry.logger << endl;
    }
  }
}
