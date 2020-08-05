#ifndef PARACOOBA_MODULE
#define PARACOOBA_MODULE

struct parac_module;
struct parac_version;
struct parac_handle;
enum parac_module_type;
enum parac_status;

struct parac_module_broker;
struct parac_module_communicator;
struct parac_module_solver;
struct parac_module_runner;

typedef void (*parac_module_exit)(struct parac_handle* handle,
                                  const char* reason);

typedef struct parac_module* (*parac_module_prepare)(
  struct parac_handle* handle);

typedef enum parac_status (*parac_module_register)(struct parac_handle* handle,
                                                   struct parac_module* module);

typedef enum parac_status (*parac_module_pre_init)(struct parac_module* module);
typedef enum parac_status (*parac_module_init)(struct parac_module* module);

typedef struct parac_version {
  int major;
  int minor;
  int patch;
  int tweak;
} parac_version;

typedef enum parac_module_type {
  PARAC_MOD_BROKER,
  PARAC_MOD_RUNNER,
  PARAC_MOD_COMMUNICATOR,
  PARAC_MOD_SOLVER
} parac_module_type;

/** @brief Called from Paracooba into the module. Controller by the module,
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

  union {
    struct parac_module_broker* broker;
    struct parac_module_runner* runner;
    struct parac_module_solver* solver;
    struct parac_module_communicator* communicator;
  };
} parac_module;

/** @brief Called from the module into Paracooba. Controller by Paracooba,
 * owned by Paracooba.
 *
 * Provides information and handles to Paracooba. Also has
 * userdata. Is always given to other functions related to the handle, so
 * the userdata is always available to Paracooba.
 */
typedef struct parac_handle {
  parac_version version;
  void* userdata;
  struct parac_config* config;

  parac_module_exit exit;
  parac_module_prepare prepare;
  parac_module_register register_module;
} parac_handle;

enum parac_status
parac_module_discover(parac_handle* handle);

#endif
