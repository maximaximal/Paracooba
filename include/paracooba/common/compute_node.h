#ifndef PARACOOBA_COMMON_COMPUTE_NODE_H
#define PARACOOBA_COMMON_COMPUTE_NODE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct parac_compute_node;
struct parac_message;
struct parac_file;
struct parac_module_solver_instance;

typedef void (*parac_compute_node_message_func)(
  struct parac_compute_node* compute_node,
  struct parac_message* msg);

typedef void (*parac_compute_node_file_func)(
  struct parac_compute_node* compute_node,
  struct parac_file* msg);

typedef void (*parac_compute_node_set_formula_parsed)(
  struct parac_compute_node* compute_node,
  parac_id originator,
  bool formula_parsed);

typedef void (*parac_compute_node_set_work_queue_size)(
  struct parac_compute_node* compute_node,
  parac_id originator,
  uint64_t work_queue_size);

typedef void (*parac_compute_node_connection_dropped_func)(
  struct parac_compute_node* compute_node);

typedef bool (*parac_compute_node_available_to_send_to_func)(
  struct parac_compute_node* compute_node);

typedef void (*parac_compute_node_free_func)(
  struct parac_compute_node* compute_node);

typedef enum parac_compute_node_state {
  PARAC_COMPUTE_NODE_NEW,
  PARAC_COMPUTE_NODE_ACTIVE,
  PARAC_COMPUTE_NODE_TIMEOUT,
  PARAC_COMPUTE_NODE_EXITED
} parac_compute_node_state;

const char*
parac_compute_node_state_to_str(parac_compute_node_state state);

typedef struct parac_compute_node {
  parac_compute_node_message_func send_message_to;     /// Set by Communicator.
  parac_compute_node_message_func receive_message_from;/// Set by Broker.

  parac_compute_node_file_func send_file_to;     /// Set by Communicator.
  parac_compute_node_file_func receive_file_from;/// Set by Broker.

  parac_compute_node_free_func broker_free;      /// Set by Broker.
  parac_compute_node_free_func communicator_free;/// Set by Communicator.

  parac_compute_node_connection_dropped_func
    connection_dropped;/// Set by Broker.
  parac_compute_node_available_to_send_to_func
    available_to_send_to;/// Set by Broker.

  parac_id id;
  parac_compute_node_state state;

  const char* connection_string;/// Set by Communicator if connection succeeds.

  void* broker_userdata;      /// Set by Broker.
  void* communicator_userdata;/// Set by Communicator.

  struct parac_module_solver_instance* solver_instance;

  uint32_t bytes_sent;
  uint32_t bytes_received;
} parac_compute_node;

#ifdef __cplusplus
}
#include <cassert>
#include <ostream>

#ifndef PARAC_LOG_INCLUDE_FMT
#define PARAC_LOG_INCLUDE_FMT
#endif

#include <paracooba/common/log.h>

class parac_compute_node_wrapper : public parac_compute_node {
  public:
  using IDConnectionStringPair = std::pair<parac_id, std::string>;

  parac_compute_node_wrapper(parac_id id = 0) {
    send_message_to = nullptr;
    receive_message_from = nullptr;
    send_file_to = nullptr;
    receive_file_from = nullptr;
    broker_free = nullptr;
    communicator_free = nullptr;
    this->id = id;
    broker_userdata = nullptr;
    communicator_userdata = nullptr;
    state = PARAC_COMPUTE_NODE_NEW;
    solver_instance = nullptr;
    connection_string = nullptr;
    connection_dropped = nullptr;
    available_to_send_to = [](parac_compute_node*) { return false; };
  }
  ~parac_compute_node_wrapper() {
    parac_log(
      PARAC_GENERAL,
      PARAC_DEBUG,
      "Deleting compute node {}. Had communicator data {}, had broker data {}",
      id,
      communicator_userdata != nullptr,
      broker_userdata != nullptr);

    if(communicator_free)
      communicator_free(this);
    if(broker_free)
      broker_free(this);
  }
  parac_compute_node_wrapper(const parac_compute_node_wrapper& o) = delete;
  parac_compute_node_wrapper(const parac_compute_node_wrapper&& o) = delete;

  IDConnectionStringPair getIDConnectionStringPair() const {
    assert(connection_string);
    return std::make_pair(id, connection_string);
  }
};

inline std::ostream&
operator<<(std::ostream& o, parac_compute_node_state state) {
  return o << parac_compute_node_state_to_str(state);
}

inline std::ostream&
operator<<(std::ostream& o, const parac_compute_node& node) {
  return o << "(" << node.id << ", " << node.state << ")";
}
#endif

#endif
