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

typedef parac_status (*parac_module_discover_func)(parac_handle*);
extern parac_module_discover_func
parac_static_module_discover(parac_module_type mod);

auto
ImportModuleDiscoverFunc(boost::filesystem::path path,
                         const std::string& name) {
  // The line below leads to a UBSan runtime error with clang.
  // Bug-report is online: https://github.com/boostorg/dll/issues/46
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
  m_handle.offsetNS = 0;
  m_handle.distrac = nullptr;
}
ModuleLoader::~ModuleLoader() {
  exit();
}

bool
ModuleLoader::load(parac_module_type type) {

  PathSource pathSource(type);
  while(!pathSource.is_complete()) {
    boost::filesystem::path path = pathSource();
    std::string name = std::string("parac_") + parac_module_type_to_str(type);
    try {
      // First, try to discover module in statically linked modules. Afterwards,
      // try dynamically loading.

      parac_status status;

      auto static_discover_func = parac_static_module_discover(type);
      if(static_discover_func) {
        status = static_discover_func(&m_handle);
        auto& mod = *m_modules[type];

        parac_log(PARAC_LOADER,
                  PARAC_DEBUG,
                  "{} named '{}' version {}.{}.{}:{} loaded from statically "
                  "linked library with status {}",
                  parac_module_type_to_str(type),
                  mod.name,
                  mod.version.major,
                  mod.version.minor,
                  mod.version.patch,
                  mod.version.tweak,
                  status);
      } else {
        auto imported = ImportModuleDiscoverFunc(path, name);
        status = imported(&m_handle);
        m_internal->imports.push_back(std::move(imported));

        auto& mod = *m_modules[type];

        parac_log(
          PARAC_LOADER,
          PARAC_DEBUG,
          "{} named '{}' version {}.{}.{}:{} loaded with from (generic) "
          "SO-path {} with status {}",
          parac_module_type_to_str(type),
          mod.name,
          mod.version.major,
          mod.version.minor,
          mod.version.patch,
          mod.version.tweak,
          (path / name).string(),
          status);
      }

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

  for(auto& mod : m_modules) {
    if(mod) {
      switch(mod->type) {
        case PARAC_MOD_BROKER:
          mod->broker = broker();
          m_handle.modules[PARAC_MOD_BROKER] = mod.get();
          break;
        case PARAC_MOD_RUNNER:
          mod->runner = runner();
          m_handle.modules[PARAC_MOD_RUNNER] = mod.get();
          break;
        case PARAC_MOD_SOLVER:
          mod->solver = solver();
          m_handle.modules[PARAC_MOD_SOLVER] = mod.get();
          break;
        case PARAC_MOD_COMMUNICATOR:
          mod->communicator = communicator();
          m_handle.modules[PARAC_MOD_COMMUNICATOR] = mod.get();
          break;
        case PARAC_MOD__COUNT:
          assert(false);
      }
      mod->handle = &m_handle;
    }
  }
  if(!isComplete()) {
    parac_log(
      PARAC_LOADER, PARAC_FATAL, "Cannot load all required paracooba modules!");
  }

  return isComplete();
}

template<typename Functor>
static bool
RunFuncInAllModules(ModuleLoader::ModuleArray& modules,
                    const char* functionName,
                    Functor getFunc) {
  parac_log(PARAC_LOADER,
            PARAC_DEBUG,
            "Running {} routines of loaded modules.",
            functionName);

  bool success = true;
  for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
    parac_module_type type = static_cast<parac_module_type>(i);
    auto& ptr = modules[type];

    if(!ptr || !getFunc(ptr)) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Cannot run {} of unloaded or incompletely initialized "
                "module {}! Skipping, to find "
                "further errors.",
                functionName,
                type);
      success = false;
      continue;
    }
    parac_log(PARAC_LOADER,
              PARAC_TRACE,
              "Running {} of module {}...",
              functionName,
              type);
    auto func = getFunc(ptr);
    parac_status s = func(ptr.get());
    if(s != PARAC_OK) {
      parac_log(PARAC_LOADER,
                PARAC_FATAL,
                "Error while running {} of module {}! Error: {}, "
                "Skipping, to find "
                "further errors.",
                functionName,
                type,
                s);
      success = false;
    }
  }
  parac_log(PARAC_LOADER,
            PARAC_DEBUG,
            "Successfully ran all {} routines of modules.",
            functionName);
  return success;
}

bool
ModuleLoader::pre_init() {
  return RunFuncInAllModules(
    m_modules, "pre_init", [](auto& p) { return p->pre_init; });
}

bool
ModuleLoader::init() {
  return RunFuncInAllModules(
    m_modules, "init", [](auto& p) { return p->init; });
}

bool
ModuleLoader::request_exit() {
  return RunFuncInAllModules(
    m_modules, "request_exit", [](auto& p) { return p->request_exit; });
}

bool
ModuleLoader::exit() {
  return RunFuncInAllModules(
    m_modules, "exit", [](auto& p) { return p->exit; });
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
  ptr->request_exit = nullptr;
  ptr->exit = nullptr;

  return ptr.get();
}
}
