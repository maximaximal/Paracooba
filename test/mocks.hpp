#include "paracooba/common/config.h"
#include "paracooba/common/log.h"
#include "paracooba/common/thread_registry.h"
#include "paracooba/common/types.h"
#include "paracooba/module.h"

#include "paracooba/broker/broker.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/runner/runner.h"
#include "paracooba/solver/solver.h"

#include <catch2/catch.hpp>
#include <cstring>

#include <chrono>
#include <thread>

typedef parac_status (*parac_module_discover_func)(parac_handle*);
extern parac_module_discover_func
parac_static_module_discover(parac_module_type mod);

class ParacoobaMock : public parac_handle {
  public:
  ParacoobaMock(parac_id id, ParacoobaMock* knownRemote = nullptr) {
    version.major = 0;
    version.minor = 0;
    version.patch = 0;
    version.tweak = 0;

    this->id = id;
    userdata = this;
    local_name = "Mock";
    host_name = "Mock";
    input_file = "";
    config = &m_config;
    thread_registry = &m_threadRegistry;
    distrac = nullptr;
    offsetNS = 0;
    prepare = [](parac_handle* handle, parac_module_type type) {
      // Can return module multiple times to ease mocking.
      ParacoobaMock* mock = static_cast<ParacoobaMock*>(handle->userdata);
      return &mock->m_modules[type];
    };

    static bool log_initialized = false;
    if(!log_initialized) {
      parac_log_init(thread_registry);
      log_initialized = true;
    }

    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      modules[i] = &m_modules[i];
    }

    std::memset(m_modules, 0, sizeof(parac_module) * PARAC_MOD__COUNT);

    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      discover(static_cast<parac_module_type>(i));
    }

    m_modules[PARAC_MOD_BROKER].broker = &m_broker;
    m_modules[PARAC_MOD_RUNNER].runner = &m_runner;
    m_modules[PARAC_MOD_SOLVER].solver = &m_solver;
    m_modules[PARAC_MOD_COMMUNICATOR].communicator = &m_communicator;

    parac_config_apply_default_values(config);

    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      auto mod = &m_modules[i];
      mod->handle = this;
      mod->pre_init(mod);
    }

    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      auto mod = &m_modules[i];
      mod->init(mod);
    }

    if(knownRemote) {
      auto c = m_modules[PARAC_MOD_COMMUNICATOR].communicator;
      c->connect_to_remote(&m_modules[PARAC_MOD_COMMUNICATOR],
                           knownRemote->getConnectionString().c_str());
    }
  }
  ~ParacoobaMock() {
    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      auto mod = &m_modules[i];
      mod->request_exit(mod);
    }

    parac_thread_registry_wait_for_exit(thread_registry);

    for(size_t i = 0; i < PARAC_MOD__COUNT; ++i) {
      auto mod = &m_modules[i];
      mod->exit(mod);
    }
  }

  void discover(parac_module_type type) {
    CAPTURE(type);
    auto func = parac_static_module_discover(type);
    REQUIRE(func);
    parac_status status = func(this);
    REQUIRE(status == PARAC_OK);
  }

  const std::string getConnectionString() {
    while(
      !m_modules[PARAC_MOD_COMMUNICATOR].communicator->tcp_acceptor_active) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return "localhost:" +
           std::to_string(
             m_modules[PARAC_MOD_COMMUNICATOR].communicator->tcp_listen_port);
  }

  parac_module_runner& getRunner() { return m_runner; }
  parac_module_communicator& getCommunicator() { return m_communicator; }
  parac_module_solver& getSolver() { return m_solver; }
  parac_module_broker& getBroker() { return m_broker; }

  private:
  paracooba::ConfigWrapper m_config;
  paracooba::ThreadRegistryWrapper m_threadRegistry;
  parac_module_runner m_runner;
  parac_module_communicator m_communicator;
  parac_module_solver m_solver;
  parac_module_broker m_broker;

  parac_module m_modules[PARAC_MOD__COUNT];
};
