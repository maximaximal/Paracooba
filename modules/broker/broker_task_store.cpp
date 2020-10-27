#include <algorithm>
#include <cassert>
#include <deque>

#include "broker_task_store.hpp"
#include "paracooba/common/path.h"
#include "paracooba/common/task.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"

namespace parac::broker {
struct TaskStore::Internal {
  std::deque<parac_task_wrapper> tasks;
};

TaskStore::TaskStore(parac_task_store& s)
  : m_internal(std::make_unique<Internal>()) {

  s.userdata = this;
  s.empty = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->empty();
  };
  s.get_size = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->size();
  };
  s.new_task = [](parac_task_store* store,
                  parac_path path,
                  parac_id originator) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->newTask(path, originator);
  };
  s.pop_top = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->pop_top();
  };
  s.pop_bottom = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->pop_bottom();
  };
}
TaskStore::~TaskStore() {}

bool
TaskStore::empty() const {
  return m_internal->tasks.empty();
}
size_t
TaskStore::size() const {
  return m_internal->tasks.size();
}
parac_task*
TaskStore::newTask(parac_path path, parac_id originator) {
  auto it = m_internal->tasks.emplace(std::lower_bound(
    m_internal->tasks.begin(),
    m_internal->tasks.end(),
    path,
    [](parac_task& t, parac_path p) { return t.path.length < p.length; }));
  return &*it;
}

parac_task*
TaskStore::pop_top() {

}
parac_task*
TaskStore::pop_bottom() {}

}
