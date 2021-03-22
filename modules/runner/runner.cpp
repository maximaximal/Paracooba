#include "paracooba/common/types.h"
#include "runner_worker_executor.hpp"
#include <cstdlib>
#include <paracooba/common/config.h>
#include <paracooba/module.h>
#include <paracooba/runner/runner.h>

#include <cassert>
#include <thread>

#include <parac_runner_export.h>

#define RUNNER_NAME "cpp_taskpuller"
#define RUNNER_VERSION_MAJOR 1
#define RUNNER_VERSION_MINOR 0
#define RUNNER_VERSION_PATCH 0
#define RUNNER_VERSION_TWEAK 0

struct RunnerUserdata {
  RunnerUserdata(parac_module& mod, parac_config_entry* entries)
    : config_entries(entries)
    , executor(mod, entries) {}

  parac_config_entry* config_entries;
  parac::runner::WorkerExecutor executor;
};

static void
init_config(RunnerUserdata* u) {
  using parac::runner::WorkerExecutor;
  parac_config_entry* e = u->config_entries;

  uint32_t worker_count = std::thread::hardware_concurrency();
  const char* workerCount = "PARAC_WORKER_COUNT";
  if(std::getenv(workerCount)) {
    worker_count = std::atoi(std::getenv(workerCount));
  }

  parac_config_entry_set_str(
    &e[WorkerExecutor::WORKER_COUNT],
    "worker-count",
    "Number of workers working on tasks that run in parallel as OS threads.");
  e[WorkerExecutor::WORKER_COUNT].registrar = PARAC_MOD_RUNNER;
  e[WorkerExecutor::WORKER_COUNT].type = PARAC_TYPE_UINT32;
  e[WorkerExecutor::WORKER_COUNT].default_value.uint32 = worker_count;
}

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->userdata);

  RunnerUserdata* runnerUserdata = static_cast<RunnerUserdata*>(mod->userdata);

  mod->runner->busy_worker_count = 0;
  mod->runner->available_worker_count = runnerUserdata->executor.workerCount();

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->userdata);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);

  RunnerUserdata* runnerUserdata = static_cast<RunnerUserdata*>(mod->userdata);
  return runnerUserdata->executor.init();
}

static parac_status
mod_request_exit(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);

  if(mod->userdata) {
    RunnerUserdata* runnerUserdata =
      static_cast<RunnerUserdata*>(mod->userdata);

    runnerUserdata->executor.exit();
  }

  return PARAC_OK;
}

static parac_status
mod_exit(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);

  if(mod->userdata) {
    RunnerUserdata* runnerUserdata =
      static_cast<RunnerUserdata*>(mod->userdata);
    delete runnerUserdata;
    mod->userdata = nullptr;
  }

  return PARAC_OK;
}

extern "C" PARAC_RUNNER_EXPORT parac_status
parac_module_discover_runner(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_RUNNER);
  assert(mod);

  mod->type = PARAC_MOD_RUNNER;
  mod->name = RUNNER_NAME;
  mod->version.major = RUNNER_VERSION_MAJOR;
  mod->version.minor = RUNNER_VERSION_MINOR;
  mod->version.patch = RUNNER_VERSION_PATCH;
  mod->version.tweak = RUNNER_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;
  mod->request_exit = mod_request_exit;
  mod->exit = mod_exit;

  parac_config_entry* entries = parac_config_reserve(
    handle->config,
    static_cast<size_t>(parac::runner::WorkerExecutor::_CONFIG_COUNT));
  assert(entries);

  RunnerUserdata* userdata = new RunnerUserdata(*mod, entries);
  mod->userdata = userdata;

  init_config(userdata);

  return PARAC_OK;
}
