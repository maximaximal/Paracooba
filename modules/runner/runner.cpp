#include <paracooba/module.h>

#include <cassert>

#include <parac_runner_export.h>

#define RUNNER_NAME "cpp_taskpuller"
#define RUNNER_VERSION_MAJOR 1
#define RUNNER_VERSION_MINOR 0
#define RUNNER_VERSION_PATCH 0
#define RUNNER_VERSION_TWEAK 0

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);

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

  return PARAC_OK;
}
