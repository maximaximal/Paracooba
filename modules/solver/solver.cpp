#include "paracooba/common/log.h"
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

  /* The solver is responsible to create the initial task and to announce the
   * result All actual solving happens in this module.
   *
   * As everything is started now, the initial task can be created. This initial
   * task is the ParserTask. Atferwards, SolverTask instances are created, that
   * work according to the CubeTree concept. The CaDiCaLManager copies the
   * original ParserTask to as many task instances as there are worker threads
   * enabled and provides the solver instances to SolverTask instances. They
   * then work and split on their stuff, accumulating data, propagating it
   * upwards and are at the original solver task in the end. This original task
   * then ends processing.
   */

  return PARAC_OK;
}

static parac_status
mod_request_exit(parac_module* mod) {
  assert(mod);
  assert(mod->solver);
  assert(mod->handle);

  return PARAC_OK;
}

static parac_status
mod_exit(parac_module* mod) {
  assert(mod);
  assert(mod->solver);
  assert(mod->handle);

  return PARAC_OK;
}

extern "C" PARAC_SOLVER_EXPORT parac_status
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
  mod->request_exit = mod_request_exit;
  mod->exit = mod_exit;

  return PARAC_OK;
}
