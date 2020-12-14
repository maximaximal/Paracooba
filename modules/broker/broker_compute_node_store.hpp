#pragma once

#include <memory>
#include <unordered_map>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/types.h>

struct parac_compute_node_store;
struct parac_handle;

namespace parac::broker {
struct ComputeNode;

class ComputeNodeStore {
  public:
  ComputeNodeStore(parac_handle& handle, parac_compute_node_store& store);
  virtual ~ComputeNodeStore();

  void updateThisNodeDescription();

  /** @brief Get a pointer to the specified compute node.
   *
   * Compute nodes are never deleted from the internal structure but only marked
   * as removed, which guarantees that a pointer stays valid for the entire
   * run-time after it once was valid.
   */
  parac_compute_node* get(parac_id id);
  parac_compute_node* get_with_connection(
    parac_id id,
    parac_compute_node_free_func communicator_free,
    void* communicator_userdata,
    parac_compute_node_message_func send_message_func,
    parac_compute_node_file_func send_file_func);
  ComputeNode* get_broker_compute_node(parac_id id);
  bool has(parac_id) const;

  void incrementThisNodeWorkQueueSize(parac_id originator);
  void decrementThisNodeWorkQueueSize(parac_id originator);

  private:
  static parac_compute_node* static_get(parac_compute_node_store* store,
                                        parac_id id);
  static parac_compute_node* static_get_with_connection(
    struct parac_compute_node_store*,
    parac_id id,
    parac_compute_node_free_func communicator_free,
    void* communicator_userdata,
    parac_compute_node_message_func send_message_func,
    parac_compute_node_file_func send_file_func);

  static bool static_has(parac_compute_node_store* store, parac_id id);

  static void static_node_free(parac_compute_node* node);

  parac_compute_node* create(parac_id id);

  void sendStatusToPeers();

  ComputeNode& thisNode();

  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_handle& m_handle;
  parac_compute_node_store& m_computeNodeStore;
};
}
