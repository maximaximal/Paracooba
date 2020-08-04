#include <paracooba/common/task_store_adapter.h>

#include <parac_common_export.h>

#include <assert.h>

static bool
forward_empty(struct parac_task_store* store) {
  assert(store);
  assert(store->userdata);
  parac_task_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_task_store_adapter used before _target field set!");
  assert(adapter->_target->empty);
  return adapter->_target->empty(adapter->_target);
}

static size_t
forward_get_size(struct parac_task_store* store) {
  assert(store);
  assert(store->userdata);
  parac_task_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_task_store_adapter used before _target field set!");
  assert(adapter->_target->get_size);
  return adapter->_target->get_size(adapter->_target);
}

static void
forward_push(struct parac_task_store* store, const struct parac_task* task) {
  assert(store);
  assert(store->userdata);
  parac_task_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_task_store_adapter used before _target field set!");
  assert(adapter->_target->push);
  adapter->_target->push(adapter->_target, task);
}

static void
forward_pop_top(struct parac_task_store* store, struct parac_task* task) {
  assert(store);
  assert(store->userdata);
  parac_task_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_task_store_adapter used before _target field set!");
  assert(adapter->_target->pop_top);
  adapter->_target->pop_top(adapter->_target, task);
}

static void
forward_pop_bottom(struct parac_task_store* store, struct parac_task* task) {
  assert(store);
  assert(store->userdata);
  parac_task_store_adapter* adapter = store->userdata;
  assert(adapter->_target &&
         "parac_task_store_adapter used before _target field set!");
  assert(adapter->_target->pop_bottom);
  adapter->_target->pop_bottom(adapter->_target, task);
}

PARAC_COMMON_EXPORT void
parac_task_store_adapter_init(parac_task_store_adapter* adapter) {
  adapter->_target = NULL;
  adapter->store.userdata = adapter;
  adapter->store.get_size = &forward_get_size;
  adapter->store.empty = &forward_empty;
  adapter->store.push = &forward_push;
  adapter->store.pop_top = &forward_pop_top;
  adapter->store.pop_bottom = &forward_pop_bottom;
}
