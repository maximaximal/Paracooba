#ifndef PARACOOBA_MODULE_COMMUNICATOR_H
#define PARACOOBA_MODULE_COMMUNICATOR_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;

typedef void (*parac_module_communicator_connect_to_remote)(
  struct parac_module*,
  const char*);

typedef struct parac_module_communicator {
  uint16_t udp_listen_port;
  uint16_t tcp_listen_port;

  bool tcp_acceptor_active;

  parac_module_communicator_connect_to_remote connect_to_remote;
} parac_module_communicator;

#ifdef __cplusplus
}
#endif

#endif
