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

  parac_id getId() const { return m_id; };
  const std::string& getInputFile() const { return m_inputFile; }

  bool distracEnabled() const { return m_enableDistrac; }
  const std::string& getDistracOutput() const { return m_distracOutput; }

  const std::string& getLocalName() const { return m_localName; }

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

  parac_id generateId(int64_t uniqueNumber);
  parac_id m_id = 0;

  std::string m_inputFile;
  std::string m_distracOutput;
  std::string m_localName;

  bool m_traceMode = false;
  bool m_debugMode = false;
  bool m_infoMode = false;
  bool m_enableDistrac = false;
};
}

#endif
