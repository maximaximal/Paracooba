#ifndef PARACOOBA_MODULE_COMMUNICATOR_H
#define PARACOOBA_MODULE_COMMUNICATOR_H

#include <paracooba/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

struct parac_module;

typedef struct parac_module_communicator {
  uint16_t udp_listen_port;
  uint16_t tcp_listen_port;
} parac_module_communicator;

#ifdef __cplusplus
}
#endif

#endif
