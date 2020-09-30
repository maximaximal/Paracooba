#include "CLI.hpp"
#include "paracooba/common/log.h"
#include <boost/program_options/variables_map.hpp>
#include <paracooba/common/config.h>
#include <paracooba/module.h>

#include <random>

#include <boost/asio/ip/host_name.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace paracooba {
CLI::CLI(struct parac_config& config)
  : m_config(config) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  std::random_device dev;
  std::mt19937_64 rng(dev());
  std::uniform_int_distribution<std::mt19937_64::result_type> dist_mac(
    -((int64_t)1 << 47), ((int64_t)1 << 47) - 1);

  std::string generatedLocalName =
    boost::asio::ip::host_name() + "_" + std::to_string(getpid());

  std::string generatedTracefileName =
    generatedLocalName + "_" +
    to_iso_extended_string(second_clock::universal_time()) + ".distrac-trace";

  // clang-format off
  m_globalOptions.add_options()
    ("help", "produce help message for all available options")
    ("id", po::value<parac_id>()->default_value(generateId(dist_mac(rng)))->value_name("int"), "Unique number for local ID")
    ("input-file", po::value<std::string>()->default_value("")->value_name("string"), "Input CNF file in DIMACS format")
    ("local-name,n", po::value<std::string>()->default_value(generatedLocalName)->value_name("string"), "Local name of this node")
    ("trace,t", po::bool_switch(&m_traceMode)->default_value(false)->value_name("bool"), "debug mode (set severity >= TRACE)")
    ("debug,d", po::bool_switch(&m_debugMode)->default_value(false)->value_name("bool"), "debug mode (set severity >= DEBG)")
    ("info,i", po::bool_switch(&m_infoMode)->default_value(false)->value_name("bool"), "debug mode (set severity >= INFO)")

    ("distrac-enable", po::bool_switch(&m_enableDistrac)->default_value(false)->value_name("bool"), "enable distrac tracing")
    ("distrac-output", po::value<std::string>()->default_value(generatedTracefileName)->value_name("string"), "distrac tracefile output")
    ;
  // clang-format on
}
CLI::~CLI() {}

void
CLI::parseConfig() {
  for(size_t i = 0; i < m_config.size; ++i) {
    parac_config_entry& entry = m_config.entries[i];
    parseConfigEntry(entry);
  }
}

void
CLI::parseConfigEntry(parac_config_entry& e) {
  auto& o = m_moduleOptions[e.registrar];

  assert(e.description);
  assert(e.name);

  switch(e.type) {
    case PARAC_TYPE_UINT64:
      o.add_options()(e.name,
                      po::value<uint64_t>()
                        ->default_value(e.default_value.uint64)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_INT64:
      o.add_options()(e.name,
                      po::value<int64_t>()
                        ->default_value(e.default_value.int64)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_UINT32:
      o.add_options()(e.name,
                      po::value<uint32_t>()
                        ->default_value(e.default_value.uint32)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_INT32:
      o.add_options()(e.name,
                      po::value<int32_t>()
                        ->default_value(e.default_value.int32)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_UINT16:
      o.add_options()(e.name,
                      po::value<uint16_t>()
                        ->default_value(e.default_value.uint16)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_INT16:
      o.add_options()(e.name,
                      po::value<int16_t>()
                        ->default_value(e.default_value.int16)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_UINT8:
      o.add_options()(e.name,
                      po::value<uint8_t>()
                        ->default_value(e.default_value.uint8)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_INT8:
      o.add_options()(e.name,
                      po::value<int8_t>()
                        ->default_value(e.default_value.int8)
                        ->value_name("int"),
                      e.description);
      break;
    case PARAC_TYPE_FLOAT:
      o.add_options()(e.name,
                      po::value<float>()
                        ->default_value(e.default_value.f)
                        ->value_name("float"),
                      e.description);
      break;
    case PARAC_TYPE_DOUBLE:
      o.add_options()(e.name,
                      po::value<double>()
                        ->default_value(e.default_value.d)
                        ->value_name("float"),
                      e.description);
      break;
    case PARAC_TYPE_STR:
      o.add_options()(
        e.name, po::value<std::string>()->value_name("string"), e.description);
      break;
    case PARAC_TYPE_VECTOR_STR:
      o.add_options()(e.name,
                      po::value<std::vector<std::string>>()
                        ->value_name("string")
                        ->multitoken(),
                      e.description);
      break;
  }
}

bool
CLI::parseGlobalArgs(int argc, char* argv[]) {

  po::positional_options_description positionalOptions;
  positionalOptions.add("input-file", 1);

  po::variables_map vm;

  try {
    po::store(po::command_line_parser(argc, argv)
                .options(m_globalOptions)
                .positional(positionalOptions)
                .allow_unregistered()
                .run(),
              vm);
    po::notify(vm);
  } catch(const std::exception& e) {
    std::cerr << "Could not parse global CLI Parameters! Error: " << e.what()
              << std::endl;
    return false;
  }

  if(m_infoMode) {
    parac_log_set_severity(PARAC_INFO);
  }
  if(m_debugMode) {
    parac_log_set_severity(PARAC_DEBUG);
  }
  if(m_traceMode) {
    parac_log_set_severity(PARAC_TRACE);
  }

  if(vm.count("input-file")) {
    m_inputFile = vm["input-file"].as<std::string>();
  }
  if(vm.count("local-name")) {
    m_localName = vm["local-name"].as<std::string>();
    parac_log_set_local_name(m_localName.c_str());
  }
  if(vm.count("id")) {
    m_id = vm["id"].as<parac_id>();
    parac_log_set_local_id(m_id);
  }
  if(vm.count("distrac-output")) {
    m_distracOutput = vm["distrac-output"].as<std::string>();
  }

  return true;
}

static const char*
ConvertStringToConstChar(const std::string& s) {
  return s.c_str();
}

static void
TryParsingCLIArgToConfigEntry(parac_config_entry& e,
                              const po::variables_map& vm) {
  assert(e.name);

  if(!vm.count(e.name))
    return;

  const auto val = vm[e.name];

  switch(e.type) {
    case PARAC_TYPE_UINT64:
      e.value.uint64 = val.as<uint64_t>();
      break;
    case PARAC_TYPE_INT64:
      e.value.int64 = val.as<int64_t>();
      break;
    case PARAC_TYPE_UINT32:
      e.value.uint32 = val.as<uint32_t>();
      break;
    case PARAC_TYPE_INT32:
      e.value.int32 = val.as<int32_t>();
      break;
    case PARAC_TYPE_UINT16:
      e.value.uint16 = val.as<uint16_t>();
      break;
    case PARAC_TYPE_INT16:
      e.value.int16 = val.as<uint16_t>();
      break;
    case PARAC_TYPE_UINT8:
      e.value.uint8 = val.as<uint8_t>();
      break;
    case PARAC_TYPE_INT8:
      e.value.int8 = val.as<int8_t>();
      break;
    case PARAC_TYPE_FLOAT:
      e.value.f = val.as<float>();
      break;
    case PARAC_TYPE_DOUBLE:
      e.value.d = val.as<double>();
      break;
    case PARAC_TYPE_STR:
      e.value.string = val.as<std::string>().c_str();
      break;
    case PARAC_TYPE_VECTOR_STR: {
      // References the strings in the m_vm member of CLI, so strings stay
      // valid.
      const auto& src = val.as<std::vector<std::string>>();
      std::vector<const char*> tgt;
      std::transform(src.begin(),
                     src.end(),
                     std::back_inserter(tgt),
                     ConvertStringToConstChar);
      e.value.string_vector.strings = tgt.data();
      e.value.string_vector.size = tgt.size();
    } break;
  }
}

bool
CLI::parseModuleArgs(int argc, char* argv[]) {
  po::options_description options;
  options.add(m_globalOptions)
    .add(m_moduleOptions[PARAC_MOD_BROKER])
    .add(m_moduleOptions[PARAC_MOD_RUNNER])
    .add(m_moduleOptions[PARAC_MOD_SOLVER])
    .add(m_moduleOptions[PARAC_MOD_COMMUNICATOR]);

  try {
    po::store(po::command_line_parser(argc, argv).options(options).run(), m_vm);
    po::notify(m_vm);
  } catch(const std::exception& e) {
    std::cerr << "Could not parse module CLI Parameters! Error: " << e.what()
              << std::endl;
    return false;
  }

  if(m_vm.count("help")) {
    std::cout << m_globalOptions << m_moduleOptions[PARAC_MOD_BROKER]
              << m_moduleOptions[PARAC_MOD_RUNNER]
              << m_moduleOptions[PARAC_MOD_SOLVER]
              << m_moduleOptions[PARAC_MOD_COMMUNICATOR] << std::endl;
    return false;
  }

  for(size_t i = 0; i < m_config.size; ++i) {
    parac_config_entry& entry = m_config.entries[i];
    TryParsingCLIArgToConfigEntry(entry, m_vm);
  }

  return true;
}

parac_id
CLI::generateId(int64_t uniqueNumber) {
  int16_t pid = std::abs(static_cast<int16_t>(::getpid()));
  return ((int64_t)pid << 48) | uniqueNumber;
}
}
