#include <paracooba/common/compute_node_store_adapter.h>

#include <parac_common_export.h>

#include <assert.h>

static struct parac_compute_node*
forward_get(struct parac_compute_node_store* store, parac_id id) {
  assert(store);
  assert(store->userdata);
  parac_compute_node_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_compute_node_store_adapter used before _target field set!");
  assert(adapter->_target->get);
  return adapter->_target->get(adapter->_target, id);
}

static bool
forward_has(struct parac_compute_node_store* store, parac_id id) {
  assert(store);
  assert(store->userdata);
  parac_compute_node_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_compute_node_store_adapter used before _target field set!");
  assert(adapter->_target->get);
  return adapter->_target->has(adapter->_target, id);
}

PARAC_COMMON_EXPORT void
parac_compute_node_store_adapter_init(
  parac_compute_node_store_adapter* adapter) {
  adapter->_target = NULL;
  adapter->store.userdata = adapter;
  adapter->store.get = &forward_get;
  adapter->store.has = &forward_has;
}
