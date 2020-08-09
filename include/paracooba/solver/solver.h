#ifndef PARACOOBA_MODULE_SOLVER_H
#define PARACOOBA_MODULE_SOLVER_H

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;
struct parac_solver_instance;

typedef void (*parac_solver_parse_cnf)(struct parac_module*);

typedef struct parac_solver_instance* (
  *parac_solver_add_instance)(struct parac_module*, parac_id originator_id);
typedef parac_status (*parac_solver_remove_instance)(
  struct parac_module*,
  struct parac_solver_instance* instance);

typedef struct parac_module_solver_instance {
  parac_id originator_id;
  parac_solver_parse_cnf parse_cnf;

  void *userdata;
} parac_module_solver_instance;

typedef struct parac_module_solver {
  parac_solver_add_instance add_instance;
  parac_solver_remove_instance remove_instance;
} parac_module_solver;

#ifdef __cplusplus
}
#endif

#endif
