#include "broker_compute_node_store.hpp"
#include <cassert>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/log.h>

#include "broker_compute_node.hpp"

namespace parac::broker {
struct ComputeNodeStore::Internal {
  std::unordered_map<parac_id, parac_compute_node_wrapper> nodes;
};

ComputeNodeStore::ComputeNodeStore(parac_compute_node_store& store)
  : m_internal(std::make_unique<Internal>()) {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Initialize ComputeNodeStore.");

  store.userdata = this;
  store.get = &ComputeNodeStore::static_get;
  store.has = &ComputeNodeStore::static_has;
}
ComputeNodeStore::~ComputeNodeStore() {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Destroy ComputeNodeStore.");
};

parac_compute_node*
ComputeNodeStore::get(parac_id id) {
  if(has(id))
    return &m_internal->nodes[id];

  // Create new node.
  return create(id);
}

ComputeNode*
ComputeNodeStore::get_broker_compute_node(parac_id id) {
  auto node = get(id);
  assert(node);
  assert(node->broker_userdata);
  return static_cast<ComputeNode*>(node->broker_userdata);
}

bool
ComputeNodeStore::has(parac_id id) const {
  return m_internal->nodes.count(id);
}

parac_compute_node*
ComputeNodeStore::create(parac_id id) {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Create compute node {}.", id);

  assert(!has(id));
  parac_compute_node_wrapper node;
  node.id = id;
  node.broker_free = &ComputeNodeStore::static_node_free;
  auto it = m_internal->nodes.try_emplace(id, std::move(node));

  auto& inserted_node = it.first->second;

  ComputeNode* broker_compute_node = new ComputeNode(it.first->second);
  inserted_node.broker_userdata = broker_compute_node;

  return &inserted_node;
}

parac_compute_node*
ComputeNodeStore::static_get(parac_compute_node_store* store, parac_id id) {
  assert(store);
  assert(store->userdata);

  ComputeNodeStore* self = static_cast<ComputeNodeStore*>(store->userdata);
  return static_cast<parac_compute_node*>(self->get(id));
}

bool
ComputeNodeStore::static_has(parac_compute_node_store* store, parac_id id) {
  assert(store);
  assert(store->userdata);

  ComputeNodeStore* self = static_cast<ComputeNodeStore*>(store->userdata);
  return self->has(id);
}
void
ComputeNodeStore::static_node_free(parac_compute_node* n) {
  assert(n);
  ComputeNode* broker_compute_node =
    static_cast<ComputeNode*>(n->broker_userdata);

  if(broker_compute_node) {
    delete broker_compute_node;
  }
}

}
