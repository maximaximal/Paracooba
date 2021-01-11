#include "paracooba/common/config.h"
#include "paracooba/common/log.h"
#include "paracooba/common/thread_registry.h"
#include "paracooba/common/types.h"
#include "paracooba/module.h"

#include "paracooba/broker/broker.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/runner/runner.h"
#include "paracooba/solver/solver.h"

#include "paracooba/loader/ModuleLoader.hpp"

#include <catch2/catch.hpp>
#include <cstdlib>
#include <cstring>

#include <chrono>
#include <thread>

typedef parac_status (*parac_module_discover_func)(parac_handle*);
extern parac_module_discover_func
parac_static_module_discover(parac_module_type mod);

class ParacoobaMock : public parac_handle {
  public:
  ParacoobaMock(
    parac_id id,
    const char* input_file = nullptr,
    ParacoobaMock* knownRemote = nullptr,
    std::set<parac_module_type> modulesToLoad = { PARAC_MOD_BROKER,
                                                  PARAC_MOD_RUNNER,
                                                  PARAC_MOD_SOLVER,
                                                  PARAC_MOD_COMMUNICATOR })
    : m_threadRegistry(id) {
    version.major = 0;
    version.minor = 0;
    version.patch = 0;
    version.tweak = 0;

    this->id = id;
    userdata = this;
    local_name = "Mock";
    host_name = "Mock";
    this->input_file = input_file;
    config = &m_config;
    thread_registry = &m_threadRegistry;
    distrac = nullptr;
    offsetNS = 0;

    modules[PARAC_MOD_BROKER] = nullptr;
    modules[PARAC_MOD_COMMUNICATOR] = nullptr;
    modules[PARAC_MOD_SOLVER] = nullptr;
    modules[PARAC_MOD_RUNNER] = nullptr;

    parac_log_init(thread_registry);

    // Load from ModuleLoader
    assert(!m_moduleLoader);
    m_moduleLoader = std::make_unique<paracooba::ModuleLoader>(
      *static_cast<parac_handle*>(this));

    m_moduleLoader->load(modulesToLoad);

    parac_config_apply_default_values(config);

    m_moduleLoader->pre_init();
    m_moduleLoader->init();

    if(knownRemote) {
      getCommunicator().connect_to_remote(
        m_moduleLoader->mod(PARAC_MOD_COMMUNICATOR),
        knownRemote->getConnectionString().c_str());
    }
  }
  ~ParacoobaMock() {
    m_moduleLoader->request_exit();
    parac_thread_registry_wait_for_exit(thread_registry);
  }

  const std::string getConnectionString() {
    while(!getCommunicator().tcp_acceptor_active(
      m_moduleLoader->mod(PARAC_MOD_COMMUNICATOR))) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return "localhost:" + std::to_string(getCommunicator().tcp_listen_port);
  }

  parac_module_runner& getRunner() { return *m_moduleLoader->runner(); }
  parac_module_communicator& getCommunicator() {
    return *m_moduleLoader->communicator();
  }
  parac_module_solver& getSolver() { return *m_moduleLoader->solver(); }
  parac_module_broker& getBroker() { return *m_moduleLoader->broker(); }

  paracooba::ThreadRegistryWrapper& getThreadRegistry() {
    return m_threadRegistry;
  }

  private:
  paracooba::ConfigWrapper m_config;
  paracooba::ThreadRegistryWrapper m_threadRegistry;
  std::unique_ptr<paracooba::ModuleLoader> m_moduleLoader;
};
