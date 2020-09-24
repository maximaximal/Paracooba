#ifndef PARACOOBA_MODULE_SOLVER_H
#define PARACOOBA_MODULE_SOLVER_H

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;
struct parac_solver_instance;

typedef parac_status (*parac_solver_instance_parse_file)(struct parac_module*,
                                                         const char* path);
typedef parac_status (*parac_solver_instance_start_solving)(
  struct parac_module*);

typedef struct parac_solver_instance* (
  *parac_solver_add_instance)(struct parac_module*, parac_id originator_id);
typedef parac_status (*parac_solver_remove_instance)(
  struct parac_module*,
  struct parac_solver_instance* instance);

typedef struct parac_module_solver_instance {
  parac_id originator_id;

  parac_solver_instance_parse_file
    parse_file;/// Called on Masters and on Daemons.
  parac_solver_instance_start_solving start_solving;/// Only called on Masters.

  void* userdata;
} parac_module_solver_instance;

typedef struct parac_module_solver {
  parac_solver_add_instance add_instance;
  parac_solver_remove_instance remove_instance;
} parac_module_solver;

#ifdef __cplusplus
}
#endif

#endif
