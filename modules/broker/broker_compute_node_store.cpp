#include "broker_compute_node_store.hpp"
#include <cassert>

#include <cstring>
#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/module.h>

#include <list>
#include <mutex>

#include "broker_compute_node.hpp"
#include "paracooba/communicator/communicator.h"
#include "paracooba/runner/runner.h"

namespace parac::broker {
struct ComputeNodeStore::Internal {
  std::mutex nodesMutex;

  std::list<parac_compute_node_wrapper> nodesList;

  std::unordered_map<parac_id,
                     std::reference_wrapper<parac_compute_node_wrapper>>
    nodesRefMap;
};

ComputeNodeStore::ComputeNodeStore(parac_handle& handle,
                                   parac_compute_node_store& store,
                                   TaskStore& taskStore)
  : m_internal(std::make_unique<Internal>())
  , m_handle(handle)
  , m_computeNodeStore(store)
  , m_taskStore(taskStore) {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Initialize ComputeNodeStore.");

  store.userdata = this;
  store.get = &ComputeNodeStore::static_get;
  store.get_with_connection = &ComputeNodeStore::static_get_with_connection;
  store.has = &ComputeNodeStore::static_has;

  parac_compute_node* thisNode = get(handle.id);
  assert(thisNode);
  assert(thisNode->broker_userdata);
  store.this_node = thisNode;

  updateThisNodeDescription();
}
ComputeNodeStore::~ComputeNodeStore() {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Destroy ComputeNodeStore.");
};

void
ComputeNodeStore::updateThisNodeDescription() {
  uint16_t udpListenPort = 0;
  uint16_t tcpListenPort = 0;
  uint32_t workers = 0;

  if(m_handle.modules[PARAC_MOD_RUNNER]) {
    assert(m_handle.modules[PARAC_MOD_RUNNER]->runner);
    auto runner = m_handle.modules[PARAC_MOD_RUNNER]->runner;
    workers = runner->available_worker_count;
  }
  if(m_handle.modules[PARAC_MOD_COMMUNICATOR]) {
    auto communicator = m_handle.modules[PARAC_MOD_COMMUNICATOR]->communicator;
    udpListenPort = communicator->udp_listen_port;
    tcpListenPort = communicator->tcp_listen_port;
  }

  thisNode().initDescription(m_handle.local_name,
                             m_handle.host_name,
                             workers,
                             tcpListenPort,
                             udpListenPort,
                             m_handle.input_file == nullptr,
                             true);
}

parac_compute_node*
ComputeNodeStore::get(parac_id id) {
  {
    std::unique_lock lock(m_internal->nodesMutex);
    auto it = m_internal->nodesRefMap.find(id);
    if(it != m_internal->nodesRefMap.end()) {
      return &it->second.get();
    }
  }

  // Create new node.
  return create(id);
}

parac_compute_node*
ComputeNodeStore::get_with_connection(
  parac_id id,
  parac_compute_node_free_func communicator_free,
  void* communicator_userdata,
  parac_compute_node_message_func send_message_func,
  parac_compute_node_file_func send_file_func) {

  parac_compute_node* n = get(id);
  n->communicator_free = communicator_free;
  n->communicator_userdata = communicator_userdata;
  n->send_message_to = send_message_func;
  n->send_file_to = send_file_func;

  parac_message_wrapper msg;
  assert(thisNode().description());
  thisNode().description()->serializeToMessage(msg);
  send_message_func(n, &msg);

  return n;
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
  std::unique_lock lock(m_internal->nodesMutex);
  return m_internal->nodesRefMap.count(id);
}

void
ComputeNodeStore::incrementThisNodeWorkQueueSize(parac_id originator) {
  thisNode().incrementWorkQueueSize(originator);
  sendStatusToPeers();
}
void
ComputeNodeStore::decrementThisNodeWorkQueueSize(parac_id originator) {
  thisNode().decrementWorkQueueSize(originator);
  sendStatusToPeers();
}
void
ComputeNodeStore::formulaParsed(parac_id originator) {
  thisNode().formulaParsed(originator);
  sendStatusToPeers();
}

parac_compute_node*
ComputeNodeStore::create(parac_id id) {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Create compute node {}.", id);

  assert(!has(id));

  std::unique_lock lock(m_internal->nodesMutex);
  auto& inserted_node = m_internal->nodesList.emplace_front();
  inserted_node.id = id;
  inserted_node.broker_free = &ComputeNodeStore::static_node_free;
  m_internal->nodesRefMap.try_emplace(id, inserted_node);

  ComputeNode* broker_compute_node =
    new ComputeNode(inserted_node, m_handle, *this, m_taskStore);
  inserted_node.broker_userdata = broker_compute_node;

  return &inserted_node;
}

void
ComputeNodeStore::sendStatusToPeers() {
  if(!thisNode().status().dirty()) {
    return;
  }
  parac_message_wrapper msg;
  thisNode().status().serializeToMessage(msg);

  std::unique_lock lock(m_internal->nodesMutex);
  for(auto& e : m_internal->nodesList) {
    if(e.send_message_to) {
      e.send_message_to(&e, &msg);
    }
  }
}

ComputeNode&
ComputeNodeStore::thisNode() {
  assert(m_computeNodeStore.this_node->broker_userdata);
  return *static_cast<ComputeNode*>(
    m_computeNodeStore.this_node->broker_userdata);
}

parac_compute_node*
ComputeNodeStore::static_get(parac_compute_node_store* store, parac_id id) {
  assert(store);
  assert(store->userdata);

  ComputeNodeStore* self = static_cast<ComputeNodeStore*>(store->userdata);
  return static_cast<parac_compute_node*>(self->get(id));
}

parac_compute_node*
ComputeNodeStore::static_get_with_connection(
  struct parac_compute_node_store* store,
  parac_id id,
  parac_compute_node_free_func communicator_free,
  void* communicator_userdata,
  parac_compute_node_message_func send_message_func,
  parac_compute_node_file_func send_file_func) {
  assert(store);
  assert(store->userdata);

  ComputeNodeStore* self = static_cast<ComputeNodeStore*>(store->userdata);
  return static_cast<parac_compute_node*>(
    self->get_with_connection(id,
                              communicator_free,
                              communicator_userdata,
                              send_message_func,
                              send_file_func));
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
