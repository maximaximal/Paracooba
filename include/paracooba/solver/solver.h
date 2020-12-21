#ifndef PARACOOBA_MODULE_SOLVER_H
#define PARACOOBA_MODULE_SOLVER_H

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;
struct parac_module_solver_instance;
struct parac_message;

typedef void (*parac_module_solver_instance_formula_parsed_cb)(
  struct parac_module_solver_instance*,
  void* userdata,
  parac_status);

typedef parac_status (*parac_module_solver_instance_handle_message)(
  struct parac_module_solver_instance*,
  struct parac_message*);

typedef parac_status (*parac_module_solver_instance_handle_formula)(
  struct parac_module_solver_instance*,
  struct parac_file*,
  void* cb_userdata,
  parac_module_solver_instance_formula_parsed_cb);

typedef void (*parac_module_solver_instance_serialize_config_to_msg)(
  struct parac_module_solver_instance*,
  struct parac_message*);

typedef struct parac_module_solver_instance* (
  *parac_module_solver_add_instance)(struct parac_module*,
                                     parac_id originator_id,
                                     struct parac_task_store*);
typedef parac_status (*parac_module_solver_remove_instance)(
  struct parac_module*,
  struct parac_module_solver_instance* instance);

typedef struct parac_module_solver_instance {
  parac_id originator_id;

  parac_module_solver_instance_handle_message
    handle_message;/// Called by Broker to create tasks or handle results from
                   /// serialized messages.

  parac_module_solver_instance_handle_formula
    handle_formula;/// Called by Broker to parse the received formula.

  parac_module_solver_instance_serialize_config_to_msg
    serialize_config_to_msg;/// Called by Broker to get config of a solver
                            /// instance.

  void* userdata;
} parac_module_solver_instance;

typedef struct parac_module_solver {
  parac_module_solver_add_instance add_instance;
  parac_module_solver_remove_instance remove_instance;
} parac_module_solver;

#ifdef __cplusplus
}
#endif

#endif
