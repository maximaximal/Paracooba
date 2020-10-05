#include "service.hpp"
#include <paracooba/module.h>

#include <cassert>

#include <parac_communicator_export.h>

#define COMMUNICATOR_NAME "cpp_asio_tcpconnections"
#define COMMUNICATOR_VERSION_MAJOR 1
#define COMMUNICATOR_VERSION_MINOR 0
#define COMMUNICATOR_VERSION_PATCH 0
#define COMMUNICATOR_VERSION_TWEAK 0

struct CommunicatorUserdata {
  CommunicatorUserdata(parac_handle& handle)
    : service(handle) {}

  parac::communicator::Service service;
};

static parac_status
pre_init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);

  CommunicatorUserdata* userdata = new CommunicatorUserdata(*mod->handle);
  mod->userdata = static_cast<void*>(userdata);

  return PARAC_OK;
}

static parac_status
init(parac_module* mod) {
  assert(mod);
  assert(mod->runner);
  assert(mod->handle);
  assert(mod->handle->config);
  assert(mod->handle->thread_registry);

  CommunicatorUserdata *userdata = static_cast<CommunicatorUserdata*>(mod->userdata);
  return userdata->service.start();
}

static parac_status mod_exit(parac_module *mod) {
  assert(mod);
  assert(mod->communicator);
  assert(mod->handle);

  CommunicatorUserdata *userdata = static_cast<CommunicatorUserdata*>(mod->userdata);
  if(userdata) {
    delete userdata;
  }

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
  mod->exit = mod_exit;

  return PARAC_OK;
}
