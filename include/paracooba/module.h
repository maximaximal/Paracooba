#ifndef PARACOOBA_MODULE_H
#define PARACOOBA_MODULE_H

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum parac_module_type {
  PARAC_MOD_COMMUNICATOR,
  PARAC_MOD_BROKER,
  PARAC_MOD_RUNNER,
  PARAC_MOD_SOLVER,
  PARAC_MOD__COUNT,
} parac_module_type;

const char*
parac_module_type_to_str(parac_module_type type);

struct parac_module;
struct parac_version;
struct parac_handle;

struct parac_module_broker;
struct parac_module_communicator;
struct parac_module_solver;
struct parac_module_runner;

typedef struct parac_module* (
  *parac_module_prepare)(struct parac_handle* handle, parac_module_type type);

typedef enum parac_status (*parac_module_register)(struct parac_handle* handle,
                                                   struct parac_module* module);

typedef enum parac_status (*parac_module_pre_init)(struct parac_module* module);
typedef enum parac_status (*parac_module_init)(struct parac_module* module);
typedef enum parac_status (*parac_module_exit)(struct parac_module* module);
typedef enum parac_status (*parac_module_request_exit)(
  struct parac_module* module);

typedef struct parac_version {
  int major;
  int minor;
  int patch;
  int tweak;
} parac_version;

/** @brief Called from Paracooba into the module. Controlled by the module,
 * owned by Paracooba.
 *
 * Provides information and handles to Paracooba. Also gives the module a space
 * for userdata. Is always given to other functions related to the module, so
 * the userdata is always available to the module.
 */
typedef struct parac_module {
  const char* name;
  parac_version version;
  parac_module_type type;
  void* userdata;
  struct parac_handle* handle;

  parac_module_pre_init pre_init;/// Called before module may fully initialize.
                                 /// Config must be setup here.
  parac_module_init
    init;/// Called after config has been parsed. Module may now fully use
         /// configuration to setup data structures, etc. on its own.

  parac_module_exit exit;/// Called when module should be unloaded at the end of
                         /// running Paracooba.

  parac_module_request_exit request_exit;/// Request the module to stop all
                                         /// threads, so it can be destroyed.

  /// Inserted once pre_init is called. Not available in initial discovery.
  union {
    struct parac_module_communicator* communicator;
    struct parac_module_broker* broker;
    struct parac_module_runner* runner;
    struct parac_module_solver* solver;
  };
} parac_module;

typedef void (*parac_handle_request_exit)(struct parac_handle* handle);

typedef bool (*parac_handle_assignment_is_set)(void* assignment_data, int l);
typedef int (*parac_handle_assignment_highest_literal)(void* assignment_data);

/** @brief Called from the module into Paracooba. Controlled by Paracooba,
 * owned by Paracooba.
 *
 * Provides information and handles to Paracooba. Also has
 * userdata. Is always given to other functions related to the handle, so
 * the userdata is always available to Paracooba.
 */
typedef struct parac_handle {
  parac_version version;
  parac_id id;
  void* userdata;
  const char* local_name;
  const char* host_name;
  const char* input_file;
  struct parac_config* config;
  struct parac_thread_registry*
    thread_registry;/// Should only be used from main thread.

  struct distrac_handle* distrac;
  int64_t offsetNS;// Offset in nanoseconds to central initiator node, used for
                   // distrac.

  parac_module_prepare prepare;// Returns a parac_module object to insert module
                               // commands into. Can only be called once.
  parac_module* modules[PARAC_MOD__COUNT];

  parac_handle_request_exit request_exit;// Request to exit program.
  parac_status exit_status;              // Exit status of program.

  // Set by solver once an assignment was found.
  parac_handle_assignment_is_set assignment_is_set;
  parac_handle_assignment_highest_literal assignment_highest_literal;
  void* assignment_data;
} parac_handle;

enum parac_status
parac_module_discover(parac_handle* handle);

#ifdef __cplusplus
}
#include <ostream>

inline std::ostream&
operator<<(std::ostream& o, parac_module_type type) {
  return o << parac_module_type_to_str(type);
}
#endif

#endif
