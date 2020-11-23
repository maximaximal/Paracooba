#include <paracooba/loader/ModuleLoader.hpp>

#include "paracooba/module.h"
#include <paracooba/common/log.h>

#include <paracooba/broker/broker.h>
#include <paracooba/communicator/communicator.h>
#include <paracooba/runner/runner.h>
#include <paracooba/solver/solver.h>

#include <boost/asio/coroutine.hpp>

#include <boost/dll.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/shared_library_load_mode.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/system_error.hpp>

#include "parac_loader_export.h"

#include <variant>

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
  Internal()
    : storedHandle(parac_handle()) {}
  Internal(parac_handle& externalHandle)
    : storedHandle(&externalHandle) {}

  std::vector<
    boost::dll::detail::import_type<decltype(parac_module_discover)>::type>
    imports;

  parac_module_broker broker;
  parac_module_runner runner;
  parac_module_solver solver;
  parac_module_communicator communicator;

  std::variant<parac_handle, parac_handle*> storedHandle;

  parac_handle& handle() {
    if(storedHandle.index() == 0)
      return std::get<parac_handle>(storedHandle);
    else {
      return *std::get<parac_handle*>(storedHandle);
    }
  }
  const parac_handle& handle() const {
    if(storedHandle.index() == 0)
      return std::get<parac_handle>(storedHandle);
    else {
      return *std::get<parac_handle*>(storedHandle);
    }
  }
};

void
ModuleLoader::static_request_exit(parac_handle* handle) {
  assert(handle);
  assert(handle->userdata);
  ModuleLoader* loader = static_cast<ModuleLoader*>(handle->userdata);
  loader->request_exit();
}

PARAC_LOADER_EXPORT
ModuleLoader::ModuleLoader(struct parac_thread_registry& thread_registry,
                           struct parac_config& config,
                           parac_id id,
                           const char* localName,
                           const char* hostName)
  : m_internal(std::make_unique<Internal>()) {
  handle().userdata = this;
  handle().prepare = &ModuleLoader::prepare;
  handle().thread_registry = &thread_registry;
  handle().config = &config;
  handle().offsetNS = 0;
  handle().distrac = nullptr;
  handle().id = id;
  handle().local_name = localName;
  handle().host_name = hostName;
  initHandle();
}
PARAC_LOADER_EXPORT
ModuleLoader::ModuleLoader(parac_handle& externalHandle)
  : m_internal(std::make_unique<Internal>(externalHandle)) {
  initHandle();
}
PARAC_LOADER_EXPORT ModuleLoader::~ModuleLoader() {
  exit();
}

void
ModuleLoader::initHandle() {
  handle().userdata = this;
  handle().prepare = &ModuleLoader::prepare;
  handle().request_exit = &static_request_exit;
  handle().exit_status = PARAC_UNKNOWN;
  handle().assignment_highest_literal = nullptr;
  handle().assignment_is_set = nullptr;
  handle().assignment_data = nullptr;
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
        status = static_discover_func(&handle());
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
        status = imported(&handle());
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

PARAC_LOADER_EXPORT struct parac_module_solver*
ModuleLoader::solver() {
  return &m_internal->solver;
}
PARAC_LOADER_EXPORT struct parac_module_runner*
ModuleLoader::runner() {
  return &m_internal->runner;
}
PARAC_LOADER_EXPORT struct parac_module_communicator*
ModuleLoader::communicator() {
  return &m_internal->communicator;
}
PARAC_LOADER_EXPORT struct parac_module_broker*
ModuleLoader::broker() {
  return &m_internal->broker;
}
PARAC_LOADER_EXPORT struct parac_module*
ModuleLoader::mod(size_t mod) {
  assert(mod < PARAC_MOD__COUNT);
  assert(m_modules[mod]);
  return m_modules[mod].get();
}

PARAC_LOADER_EXPORT bool
ModuleLoader::load(std::set<parac_module_type> modulesToLoad) {
  for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
    parac_module_type type = static_cast<parac_module_type>(i);
    if(modulesToLoad.count(type)) {
      load(type);
    }
  }

  for(auto& mod : m_modules) {
    if(mod) {
      switch(mod->type) {
        case PARAC_MOD_BROKER:
          mod->broker = broker();
          handle().modules[PARAC_MOD_BROKER] = mod.get();
          break;
        case PARAC_MOD_RUNNER:
          mod->runner = runner();
          handle().modules[PARAC_MOD_RUNNER] = mod.get();
          break;
        case PARAC_MOD_SOLVER:
          mod->solver = solver();
          handle().modules[PARAC_MOD_SOLVER] = mod.get();
          break;
        case PARAC_MOD_COMMUNICATOR:
          mod->communicator = communicator();
          handle().modules[PARAC_MOD_COMMUNICATOR] = mod.get();
          break;
        case PARAC_MOD__COUNT:
          assert(false);
      }
      mod->handle = &handle();
    }
  }
  if(!isComplete(modulesToLoad)) {
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

    // If the module is not loaded at this stage, it is explicitly deactivated.
    if(!ptr)
      return true;

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

PARAC_LOADER_EXPORT bool
ModuleLoader::pre_init() {
  return RunFuncInAllModules(
    m_modules, "pre_init", [](auto& p) { return p->pre_init; });
}

PARAC_LOADER_EXPORT bool
ModuleLoader::init() {
  return RunFuncInAllModules(
    m_modules, "init", [](auto& p) { return p->init; });
}

PARAC_LOADER_EXPORT bool
ModuleLoader::request_exit() {
  return RunFuncInAllModules(
    m_modules, "request_exit", [](auto& p) { return p->request_exit; });
}

PARAC_LOADER_EXPORT bool
ModuleLoader::exit() {
  return RunFuncInAllModules(
    m_modules, "exit", [](auto& p) { return p->exit; });
}

PARAC_LOADER_EXPORT bool
ModuleLoader::isComplete(std::set<parac_module_type> modulesToLoad) {
  for(parac_module_type t : modulesToLoad) {
    if(!m_modules[t])
      return false;
  }
  return true;
}

PARAC_LOADER_EXPORT bool
ModuleLoader::hasSolver() {
  return (bool)m_modules[PARAC_MOD_SOLVER];
}
PARAC_LOADER_EXPORT bool
ModuleLoader::hasBroker() {
  return (bool)m_modules[PARAC_MOD_BROKER];
}
PARAC_LOADER_EXPORT bool
ModuleLoader::hasRunner() {
  return (bool)m_modules[PARAC_MOD_RUNNER];
}
PARAC_LOADER_EXPORT bool
ModuleLoader::hasCommunicator() {
  return (bool)m_modules[PARAC_MOD_COMMUNICATOR];
}

PARAC_LOADER_EXPORT parac_module*
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

PARAC_LOADER_EXPORT parac_handle&
ModuleLoader::handle() {
  return m_internal->handle();
}
PARAC_LOADER_EXPORT const parac_handle&
ModuleLoader::handle() const {
  return m_internal->handle();
}
}
