#pragma once

#include <memory>
#include <unordered_map>

#include <paracooba/common/types.h>

struct parac_compute_node_store;
struct parac_compute_node;
struct parac_compute_node_wrapper;
struct parac_handle;

namespace parac::broker {
struct ComputeNode;

class ComputeNodeStore {
  public:
  ComputeNodeStore(parac_handle& handle, parac_compute_node_store& store);
  ~ComputeNodeStore();

  /** @brief Get a pointer to the specified compute node.
   *
   * Compute nodes are never deleted from the internal structure but only marked
   * as removed, which guarantees that a pointer stays valid for the entire
   * run-time after it once was valid.
   */
  parac_compute_node* get(parac_id id);
  ComputeNode* get_broker_compute_node(parac_id id);
  bool has(parac_id) const;

  private:
  static parac_compute_node* static_get(parac_compute_node_store* store,
                                        parac_id id);
  static bool static_has(parac_compute_node_store* store, parac_id id);

  static void static_node_free(parac_compute_node* node);

  parac_compute_node* create(parac_id id);

  struct Internal;
  std::unique_ptr<Internal> m_internal;

  parac_handle &m_handle;
};
}
