#include <algorithm>
#include <cassert>
#include <deque>
#include <list>
#include <set>
#include <unordered_map>

#include "broker_task_store.hpp"
#include "paracooba/common/compute_node.h"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/task.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"

#include <boost/pool/pool_alloc.hpp>

namespace parac::broker {
struct TaskStore::Internal {
  std::mutex containerMutex;

  struct Task {
    parac_task t;

    ~Task() {
      if(t.free_userdata) {
        t.free_userdata(&t);
      }
    }
  };
  using TaskRef = std::reference_wrapper<parac_task>;

  template<typename T>
  using Allocator = boost::fast_pool_allocator<
    T,
    boost::default_user_allocator_new_delete,
    boost::details::pool::null_mutex,// No mutex required, as accesses are
                                     // synchronized by containerMutex
    64,
    128>;

  struct TaskRefCompare {
    bool operator()(const TaskRef& lhs, const TaskRef& rhs) const {
      return static_cast<parac::Path&>(lhs.get().path) <
             static_cast<parac::Path&>(rhs.get().path);
    }
  };

  std::list<Task, Allocator<Task>> tasks;

  std::list<TaskRef, Allocator<TaskRef>> tasksWaitingForWorkerQueue;

  std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>> tasksBeingWorkedOn;
  std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>> tasksWaitingForChildren;

  std::unordered_map<parac_compute_node*,
                     std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>>>
    offloadedTasks;
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
  s.get_waiting_for_worker_size = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->m_internal->tasksWaitingForWorkerQueue.size();
  };
  s.get_waiting_for_children_size = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->m_internal->tasksWaitingForChildren.size();
  };
  s.new_task = [](parac_task_store* store, parac_task* creator_task) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->newTask(creator_task);
  };
  s.pop_offload = [](parac_task_store* store, parac_compute_node* target) {
    assert(store);
    assert(store->userdata);
    assert(target);
    return static_cast<TaskStore*>(store->userdata)->pop_offload(target);
  };
  s.pop_work = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)->pop_work();
  };
  s.assess_task = [](parac_task_store* store, parac_task* task) {
    assert(store);
    assert(store->userdata);
    assert(task);
    return static_cast<TaskStore*>(store->userdata)->assess_task(task);
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
TaskStore::newTask(parac_task* parent_task) {
  std::unique_lock lock(m_internal->containerMutex);
  parac_task* task = &m_internal->tasks.emplace_front().t;
  parac_task_init(task);
  task->parent_task = parent_task;
  return task;
}

parac_task*
TaskStore::pop_offload(parac_compute_node* target) {
  std::unique_lock lock(m_internal->containerMutex);
  if(m_internal->tasksWaitingForWorkerQueue.empty()) {
    return nullptr;
  }
  parac_task& t = m_internal->tasksWaitingForWorkerQueue.front();
  m_internal->tasksWaitingForWorkerQueue.pop_front();
  m_internal->offloadedTasks[target].insert(t);
  return &t;
}
parac_task*
TaskStore::pop_work() {
  std::unique_lock lock(m_internal->containerMutex);
  if(m_internal->tasksWaitingForWorkerQueue.empty()) {
    return nullptr;
  }
  parac_task& t = m_internal->tasksWaitingForWorkerQueue.front();
  m_internal->tasksWaitingForWorkerQueue.pop_front();
  m_internal->tasksBeingWorkedOn.insert(t);
  return &t;
}

void
TaskStore::insert_into_tasksWaitingForWorkerQueue(parac_task* task) {
  assert(task);
  std::unique_lock lock(m_internal->containerMutex);
  m_internal->tasksWaitingForWorkerQueue.insert(
    std::lower_bound(m_internal->tasksWaitingForWorkerQueue.begin(),
                     m_internal->tasksWaitingForWorkerQueue.end(),
                     task->path,
                     [](const Internal::TaskRef& t, parac_path p) {
                       return t.get().path.length < p.length;
                     }),
    *task);
}

void
TaskStore::insert_into_tasksWaitingForChildren(parac_task* task) {
  assert(task);
  std::unique_lock lock(m_internal->containerMutex);
  m_internal->tasksWaitingForChildren.insert(*task);
}
void
TaskStore::remove_from_tasksWaitingForChildren(parac_task* task) {
  assert(task);
  std::unique_lock lock(m_internal->containerMutex);
  m_internal->tasksWaitingForChildren.erase(*task);
}

void
TaskStore::assess_task(parac_task* task) {
  assert(task);
  assert(task->assess);
  parac_task_state s = task->assess(task);

  parac_log(PARAC_BROKER,
            PARAC_TRACE,
            "Assessing task on path {} to state {}",
            task->path,
            task->state);

  if(s & PARAC_TASK_WORK_AVAILABLE) {
    insert_into_tasksWaitingForWorkerQueue(task);
  }
  if(!(task->last_state & PARAC_TASK_WAITING_FOR_SPLITS) &&
     s & PARAC_TASK_WAITING_FOR_SPLITS) {
    insert_into_tasksWaitingForChildren(task);
  }
  if((s & PARAC_TASK_SPLITS_DONE) == PARAC_TASK_SPLITS_DONE) {
    remove_from_tasksWaitingForChildren(task);
  }
  if((!(s & PARAC_TASK_SPLITTED) && s & PARAC_TASK_DONE) ||
     ((s & PARAC_TASK_SPLITTED) &&
      (s & PARAC_TASK_ALL_DONE) == PARAC_TASK_ALL_DONE)) {
    assert(task->result != PARAC_PENDING);

    if(task->parent_task &&
       m_internal->tasksWaitingForChildren.count(*task->parent_task)) {
      parac_task* parent = task->parent_task;
      Path path(task->path);
      Path parentPath(parent->path);

      if(path == parentPath.left()) {
        parent->left_result = task->result;
      } else if(path == parentPath.right()) {
        parent->right_result = task->result;
      } else {
        parac_log(PARAC_BROKER,
                  PARAC_GLOBALERROR,
                  "Task on path {} specified parent path on path {}, but is no "
                  "direct child of parent! Ignoring parent.",
                  path,
                  parentPath);
        return;
      }

      assess_task(parent);
    }
  }

  task->last_state = s;
}

}
