#include "paracooba/common/config.h"
#include "paracooba/common/types.h"
#include <paracooba/module.h>

#define PARAC_LOG_INCLUDE_FMT
#include "paracooba/common/log.h"

#include <cassert>

#include <parac_broker_export.h>

#define BROKER_NAME "cpp_nodemap_keepstocked"
#define BROKER_VERSION_MAJOR 1
#define BROKER_VERSION_MINOR 0
#define BROKER_VERSION_PATCH 0
#define BROKER_VERSION_TWEAK 0

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

PARAC_BROKER_EXPORT parac_status
parac_module_discover_broker(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_BROKER);
  assert(mod);

  mod->type = PARAC_MOD_BROKER;
  mod->name = BROKER_NAME;
  mod->version.major = BROKER_VERSION_MAJOR;
  mod->version.minor = BROKER_VERSION_MINOR;
  mod->version.patch = BROKER_VERSION_PATCH;
  mod->version.tweak = BROKER_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;

  return PARAC_OK;
}

PARAC_BROKER_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_broker(handle);
}
