#include <paracooba/module.h>

#include <cassert>

#include <parac_communicator_export.h>

#define COMMUNICATOR_NAME "cpp_asio_tcpconnections"
#define COMMUNICATOR_VERSION_MAJOR 1
#define COMMUNICATOR_VERSION_MINOR 0
#define COMMUNICATOR_VERSION_PATCH 0
#define COMMUNICATOR_VERSION_TWEAK 0

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

extern "C" PARAC_COMMUNICATOR_EXPORT parac_status
parac_module_discover_communicator(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_COMMUNICATOR);
  assert(mod);

  mod->type = PARAC_MOD_COMMUNICATOR;
  mod->name = COMMUNICATOR_NAME;
  mod->version.major = COMMUNICATOR_VERSION_MAJOR;
  mod->version.minor = COMMUNICATOR_VERSION_MINOR;
  mod->version.patch = COMMUNICATOR_VERSION_PATCH;
  mod->version.tweak = COMMUNICATOR_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;

  return PARAC_OK;
}
