#include "ModuleLoader.hpp"
#include "paracooba/module.h"
#include <paracooba/common/log.h>

#include "paracooba/broker/broker.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/runner/runner.h"
#include "paracooba/solver/solver.h"

#include <boost/asio/coroutine.hpp>

#include <boost/dll.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/shared_library_load_mode.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>

auto
ImportModuleDiscoverFunc(boost::filesystem::path path,
                         const std::string& name) {
  return boost::dll::import<decltype(parac_module_discover)>(
    path / name,
    "parac_module_discover",
    boost::dll::load_mode::append_decorations);
}

struct PathSource : public boost::asio::coroutine {
  PathSource(parac_module_type type)
    : type(type) {}

#include <boost/asio/yield.hpp>
  boost::filesystem::path operator()() {
    reenter(this) {
      yield return boost::filesystem::current_path();
      yield return boost::filesystem::current_path() /
        parac_module_type_to_str(type);
      yield return boost::filesystem::current_path() / "modules" /
        parac_module_type_to_str(type);
      yield return boost::dll::program_location().parent_path();
      yield return boost::dll::program_location().parent_path() /
        parac_module_type_to_str(type);
      yield return boost::dll::program_location().parent_path() / "modules" /
        parac_module_type_to_str(type);
      yield return "/usr/local/lib/paracooba";
      yield return "/usr/local/lib";
      yield return "/usr/lib/paracooba";
      yield return "/usr/lib";
    }
    return "";
  }
#include <boost/asio/unyield.hpp>

  parac_module_type type;
};

namespace paracooba {
struct ModuleLoader::Internal {
  std::vector<
    boost::dll::detail::import_type<decltype(parac_module_discover)>::type>
    imports;

  parac_module_broker broker;
  parac_module_runner runner;
  parac_module_solver solver;
  parac_module_communicator communicator;
};

ModuleLoader::ModuleLoader(struct parac_thread_registry& thread_registry,
                           struct parac_config& config)
  : m_internal(std::make_unique<Internal>()) {
  m_handle.userdata = this;
  m_handle.prepare = &ModuleLoader::prepare;
  m_handle.thread_registry = &thread_registry;
  m_handle.config = &config;
}
ModuleLoader::~ModuleLoader() {}

bool
ModuleLoader::load(parac_module_type type) {

  PathSource pathSource(type);
  while(!pathSource.is_complete()) {
    boost::filesystem::path path = pathSource();
    std::string name = std::string("parac_") + parac_module_type_to_str(type);
    try {
      auto imported = ImportModuleDiscoverFunc(path, name);
      imported(&m_handle);

      auto& mod = *m_modules[type];

      parac_log(PARAC_LOADER,
                PARAC_DEBUG,
                "{} named '{}' version {}.{}.{}:{} loaded with from (generic) "
                "SO-path {}",
                parac_module_type_to_str(type),
                mod.name,
                mod.version.major,
                mod.version.minor,
                mod.version.patch,
                mod.version.tweak,
                (path / name).string());

      m_internal->imports.push_back(std::move(imported));

      return true;
    } catch(boost::system::system_error& err) {
      (void)err;
      // Ignoring failed loads, because multiple locations are tried.
      parac_log(PARAC_LOADER,
                PARAC_TRACE,
                "{} could not be loaded! Message: {}",
                parac_module_type_to_str(type),
                err.what());
    }
  }

  parac_log(PARAC_LOADER,
            PARAC_FATAL,
            "{} could not be loaded!",
            parac_module_type_to_str(type));
  return false;
}

struct parac_module_solver*
ModuleLoader::solver() {
  return &m_internal->solver;
}
struct parac_module_runner*
ModuleLoader::runner() {
  return &m_internal->runner;
}
struct parac_module_communicator*
ModuleLoader::communicator() {
  return &m_internal->communicator;
}
struct parac_module_broker*
ModuleLoader::broker() {
  return &m_internal->broker;
}

bool
ModuleLoader::load() {
  for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
    parac_module_type type = static_cast<parac_module_type>(i);
    load(type);
  }

  if(isComplete()) {
    for(auto& mod : m_modules) {
      mod->broker = broker();
      mod->runner = runner();
      mod->communicator = communicator();
      mod->solver = solver();
      mod->handle = &m_handle;
    }
  } else {
    parac_log(
      PARAC_LOADER, PARAC_FATAL, "Cannot load all required paracooba modules!");
  }

  return isComplete();
}

bool
ModuleLoader::pre_init() {
  parac_log(
    PARAC_LOADER, PARAC_DEBUG, "Running pre_init routines of loaded modules.");

  bool success = true;
  for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
    parac_module_type type = static_cast<parac_module_type>(i);
    auto& ptr = m_modules[type];
    if(!ptr || !ptr->pre_init) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Cannot run pre_init of unloaded or incompletely initialized "
                "module {}! Skipping, to find "
                "further errors.",
                type);
      success = false;
      continue;
    }
    parac_log(
      PARAC_LOADER, PARAC_TRACE, "Running pre_init of module {}...", type);
    parac_status s = ptr->pre_init(ptr.get());
    if(s != PARAC_OK) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Error while running pre_init of module {}! Error: {}, "
                "Skipping, to find "
                "further errors.",
                type,
                s);
    }
    success = false;
  }
  parac_log(PARAC_LOADER,
            PARAC_DEBUG,
            "Successfully ran all pre_init routines of modules.");
  return success;
}

bool
ModuleLoader::init() {
  parac_log(
    PARAC_LOADER, PARAC_DEBUG, "Running init routines of loaded modules.");

  bool success = true;
  for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
    parac_module_type type = static_cast<parac_module_type>(i);
    auto& ptr = m_modules[type];
    if(!ptr || !ptr->init) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Cannot run init of unloaded or incompletely initialized "
                "module {}! Skipping, to find "
                "further errors.",
                type);
      success = false;
      continue;
    }
    parac_log(PARAC_LOADER, PARAC_TRACE, "Running init of module {}...", type);
    parac_status s = ptr->init(ptr.get());
    if(s != PARAC_OK) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Error while running init of module {}! Error: {}, "
                "Skipping, to find "
                "further errors.",
                type,
                s);
    }
    success = false;
  }
  parac_log(PARAC_LOADER,
            PARAC_DEBUG,
            "Successfully ran all init routines of modules.");
  return success;
}

bool
ModuleLoader::isComplete() {
  return hasSolver() && hasRunner() && hasCommunicator() && hasBroker();
}

bool
ModuleLoader::hasSolver() {
  return (bool)m_modules[PARAC_MOD_SOLVER];
}
bool
ModuleLoader::hasBroker() {
  return (bool)m_modules[PARAC_MOD_BROKER];
}
bool
ModuleLoader::hasRunner() {
  return (bool)m_modules[PARAC_MOD_RUNNER];
}
bool
ModuleLoader::hasCommunicator() {
  return (bool)m_modules[PARAC_MOD_COMMUNICATOR];
}

parac_module*
ModuleLoader::prepare(parac_handle* handle, parac_module_type type) {
  assert(handle);
  assert(handle->userdata);
  ModuleLoader* self = static_cast<ModuleLoader*>(handle->userdata);

  auto& ptr = self->m_modules[type];
  if(ptr) {
    parac_log(PARAC_LOADER,
              PARAC_FATAL,
              "Prepare called more than once for module {}!",
              parac_module_type_to_str(type));
    return nullptr;
  }

  ptr = std::make_unique<parac_module>();

  ptr->pre_init = nullptr;
  ptr->init = nullptr;

  return ptr.get();
}
}
