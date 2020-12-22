#ifndef PARACOOBA_MODULE_COMMUNICATOR_H
#define PARACOOBA_MODULE_COMMUNICATOR_H

#include <paracooba/common/status.h>
#include <paracooba/common/timeout.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;

typedef void (*parac_module_communicator_connect_to_remote)(
  struct parac_module*,
  const char*);

typedef bool (*parac_module_communicator_is_active)(struct parac_module*);

typedef struct parac_timeout* (*parac_module_communicator_set_timeout)(
  struct parac_module*,
  uint64_t ms,
  void* userdata,
  parac_timeout_expired expiery_cb);

typedef struct parac_module_communicator {
  uint16_t udp_listen_port;
  uint16_t tcp_listen_port;

  parac_module_communicator_is_active tcp_acceptor_active;

  parac_module_communicator_connect_to_remote connect_to_remote;
  parac_module_communicator_set_timeout set_timeout;
} parac_module_communicator;

#ifdef __cplusplus
}
#endif

#endif
