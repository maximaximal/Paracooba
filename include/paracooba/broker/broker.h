#ifndef PARACOOBA_MODULE_BROKER
#define PARACOOBA_MODULE_BROKER

struct parac_task_store;
struct parac_compute_node_store;
struct parac_compute_node;
struct parac_module;
enum parac_status;

typedef struct parac_task_store* (*parac_broker_get_task_store)(
  struct parac_module*);
typedef struct parac_compute_node_store* (*parac_broker_get_compute_node_store)(
  struct parac_module*);

typedef struct parac_module_broker
{
  parac_broker_get_task_store get_task_store;
  parac_broker_get_compute_node_store get_compute_node_store;
} parac_module_broker;

#endif
