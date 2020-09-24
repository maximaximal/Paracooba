#ifndef PARAC_EXECUTABLE_CLI_HPP
#define PARAC_EXECUTABLE_CLI_HPP

#include "paracooba/common/config.h"
#include <paracooba/module.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

namespace paracooba {
class CLI {
  public:
  CLI(struct parac_config& config);
  ~CLI();

  void parseConfig();

  bool parseGlobalArgs(int argc, char* argv[]);
  bool parseModuleArgs(int argc, char* argv[]);

  int64_t getId() const { return m_id; };
  const std::string& getInputFile() const { return m_inputFile; }

  private:
  boost::program_options::options_description m_globalOptions{
    "Global Options"
  };
  boost::program_options::options_description
    m_moduleOptions[PARAC_MOD__COUNT] = {
      boost::program_options::options_description("Broker Options"),
      boost::program_options::options_description("Runner Options"),
      boost::program_options::options_description("Communicator Options"),
      boost::program_options::options_description("Solver Options"),
    };

  boost::program_options::variables_map m_vm;

  struct parac_config& m_config;

  void parseConfigEntry(struct parac_config_entry& e);

  int64_t generateId(int64_t uniqueNumber);
  int64_t m_id = 0;

  std::string m_inputFile;

  bool m_traceMode = false;
  bool m_debugMode = false;
  bool m_infoMode = false;
};
}

#endif
