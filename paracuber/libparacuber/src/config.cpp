#include "../include/paracuber/config.hpp"
#include <boost/program_options.hpp>
#include <cstddef>
#include <iostream>

namespace po = boost::program_options;

namespace paracuber {
Config::Config()
  : m_optionsCLI("CLI-only options")
  , m_optionsCommon("CLI and config file options")
  , m_optionsFile("Config file options")
{
  /* CLI ONLY OPTIONS
   * --------------------------------------- */
  // clang-format off
  m_optionsCLI.add_options()
    ("help", "produce help message")
    ;
  // clang-format on

  /* CONFIG FILES ONLY OPTIONS
   * --------------------------------------- */
  // clang-forma // clang-format on

  /* COMMON OPTIONS
   * --------------------------------------- */
  // clang-format off
  m_optionsCommon.add_options()
    (GetConfigNameFromEnum(Config::LocalName),
         po::value<std::string>()->default_value("Unnamed"), "local name of this solver node")
    (GetConfigNameFromEnum(Config::InputFile),
         po::value<std::string>()->default_value(""), "input file (problem) to parse")
    ("debug,d", po::bool_switch(&m_debugMode)->default_value(false), "debug mode")
    ("daemon", po::bool_switch(&m_daemonMode)->default_value(false), "daemon mode")
    ;
  // clang-format on
}

Config::~Config() {}

bool
Config::parseParameters(int argc, char** argv)
{
  po::positional_options_description positionalOptions;
  positionalOptions.add("input-file", 1);

  po::options_description cliGroup;
  cliGroup.add(m_optionsCommon).add(m_optionsCLI);
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(cliGroup)
              .positional(positionalOptions)
              .run(),
            vm);
  po::notify(vm);

  if(vm.count("help")) {
    std::cout << m_optionsCLI << std::endl;
    std::cout << m_optionsCommon << std::endl;
    std::cout << m_optionsFile << std::endl;
    return false;
  }
  return processCommonParameters(vm);
}

bool
Config::parseConfigFile(std::string_view filePath)
{
  return true;
}

template<typename T>
inline void
conditionallySetConfigOptionToArray(
  const boost::program_options::variables_map& vm,
  std::any* arr,
  Config::Key key)
{
  if(vm.count(GetConfigNameFromEnum(key))) {
    arr[key] = vm[GetConfigNameFromEnum(key)].as<T>();
  }
}

bool
Config::processCommonParameters(const boost::program_options::variables_map& vm)
{
  conditionallySetConfigOptionToArray<std::string>(
    vm, m_config.data(), Config::LocalName);
  conditionallySetConfigOptionToArray<std::string>(
    vm, m_config.data(), Config::InputFile);

  return true;
}
}
