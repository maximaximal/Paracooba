#include <paracooba/module.h>

#include <cassert>

#include <parac_solver_export.h>

#define SOLVER_NAME "cpp_cubetree_splitter"
#define SOLVER_VERSION_MAJOR 1
#define SOLVER_VERSION_MINOR 0
#define SOLVER_VERSION_PATCH 0
#define SOLVER_VERSION_TWEAK 0

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

PARAC_SOLVER_EXPORT parac_status
parac_module_discover_solver(parac_handle* handle) {
  assert(handle);

  parac_module* mod = handle->prepare(handle, PARAC_MOD_SOLVER);
  assert(mod);

  mod->type = PARAC_MOD_SOLVER;
  mod->name = SOLVER_NAME;
  mod->version.major = SOLVER_VERSION_MAJOR;
  mod->version.minor = SOLVER_VERSION_MINOR;
  mod->version.patch = SOLVER_VERSION_PATCH;
  mod->version.tweak = SOLVER_VERSION_TWEAK;
  mod->pre_init = pre_init;
  mod->init = init;

  return PARAC_OK;
}

PARAC_SOLVER_EXPORT parac_status
parac_module_discover(parac_handle* handle) {
  return parac_module_discover_solver(handle);
}
