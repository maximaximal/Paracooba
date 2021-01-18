#include <algorithm>
#include <atomic>
#include <cassert>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

#include "broker_compute_node_store.hpp"
#include "broker_task_store.hpp"

#include "paracooba/broker/broker.h"
#include "paracooba/common/compute_node.h"
#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/log.h"
#include "paracooba/common/message.h"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/common/task.h"
#include "paracooba/common/task_store.h"
#include "paracooba/common/types.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/module.h"

#include <boost/pool/pool_alloc.hpp>

namespace parac::broker {
static ComputeNodeStore&
extractComputeNodeStore(parac_handle& h) {
  assert(h.modules[PARAC_MOD_BROKER]);
  parac_module_broker& broker = *h.modules[PARAC_MOD_BROKER]->broker;
  assert(broker.compute_node_store);
  assert(broker.compute_node_store->userdata);
  return *static_cast<ComputeNodeStore*>(broker.compute_node_store->userdata);
}

static void
incrementWorkQueueInComputeNode(parac_handle& h, parac_id originator) {
  ComputeNodeStore& brokerComputeNodeStore = extractComputeNodeStore(h);
  brokerComputeNodeStore.incrementThisNodeWorkQueueSize(originator);
}

static void
decrementWorkQueueInComputeNode(parac_handle& h, parac_id originator) {
  ComputeNodeStore& brokerComputeNodeStore = extractComputeNodeStore(h);
  brokerComputeNodeStore.decrementThisNodeWorkQueueSize(originator);
}

struct TaskStore::Internal {
  explicit Internal(parac_handle& handle, parac_task_store& store)
    : handle(handle)
    , store(store) {}

  std::recursive_mutex assessMutex;
  std::mutex containerMutex;
  parac_handle& handle;
  parac_task_store& store;

  struct Task;
  template<typename T>
  using Allocator =
    boost::fast_pool_allocator<T,
                               boost::default_user_allocator_new_delete,
                               boost::details::pool::default_mutex,
                               64,
                               128>;
  using TaskList = std::list<Task, Allocator<Task>>;

  struct Task {
    TaskList::iterator it;
    parac_task t;
    short refcount = 0;

    ~Task() {
      parac_log(PARAC_BROKER,
                PARAC_TRACE,
                "Delete task on path {} with address {}",
                t.path,
                static_cast<void*>(&t));
      if(t.free_userdata) {
        t.free_userdata(&t);
      }
    }
  };
  using TaskRef = std::reference_wrapper<parac_task>;

  struct TaskRefCompare {
    bool operator()(const TaskRef& lhs, const TaskRef& rhs) const {
      return static_cast<parac::Path&>(lhs.get().path) >
             static_cast<parac::Path&>(rhs.get().path);
    }
  };

  TaskList tasks;

  std::list<TaskRef, Allocator<TaskRef>> tasksWaitingForWorkerQueue;

  std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>> tasksBeingWorkedOn;
  std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>> tasksWaitingForChildren;

  std::unordered_map<parac_compute_node*,
                     std::unordered_map<parac_path_type,
                                        TaskRef,
                                        std::hash<parac_path_type>,
                                        std::equal_to<parac_path_type>,
                                        Allocator<TaskRef>>>
    offloadedTasks;

  std::atomic_size_t tasksSize = 0;
  std::atomic_size_t tasksWaitingForWorkerQueueSize = 0;

  parac_timeout* autoShutdownTimeout = nullptr;
};

TaskStore::TaskStore(parac_handle& handle,
                     parac_task_store& s,
                     uint32_t autoShutdownTimeout)
  : m_internal(std::make_unique<Internal>(handle, s))
  , m_autoShutdownTimeout(autoShutdownTimeout) {

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
      ->m_internal->tasksWaitingForWorkerQueueSize.load();
  };
  s.get_waiting_for_children_size = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->m_internal->tasksWaitingForChildren.size();
  };
  s.get_tasks_being_worked_on_size = [](parac_task_store* store) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->m_internal->tasksBeingWorkedOn.size();
  };
  s.new_task = [](parac_task_store* store,
                  parac_task* creator_task,
                  parac_path new_path,
                  parac_id originator) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->newTask(creator_task, new_path, originator);
  };
  s.undo_offload = [](parac_task_store* store, parac_task* t) {
    assert(store);
    assert(store->userdata);
    assert(t);
    return static_cast<TaskStore*>(store->userdata)->undo_offload(t);
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
  s.ping_on_work_userdata = nullptr;
  s.ping_on_work = nullptr;
}
TaskStore::~TaskStore() {
  parac_log(PARAC_BROKER, PARAC_DEBUG, "Destroy TaskStore.");
}

bool
TaskStore::empty() const {
  return m_internal->tasks.empty();
}
size_t
TaskStore::size() const {
  return m_internal->tasks.size();
}
parac_task*
TaskStore::newTask(parac_task* parent_task,
                   parac_path new_path,
                   parac_id originator) {
  std::unique_lock lock(m_internal->containerMutex);
  auto& taskWrapper = m_internal->tasks.emplace_front();
  parac_task* task = &taskWrapper.t;
  taskWrapper.it = m_internal->tasks.begin();
  parac_task_init(task);
  task->parent_task_ = parent_task;
  task->task_store = &m_internal->store;
  task->path = new_path;
  task->originator = originator;

  if(parent_task) {
    Internal::Task* parentWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(parent_task) - offsetof(Internal::Task, t));
    ++(parentWrapper->refcount);

    if(parac_path_get_next_left(parent_task->path).rep == new_path.rep) {
      parent_task->left_child_ = task;
    } else if(parac_path_get_next_right(parent_task->path).rep ==
              new_path.rep) {
      parent_task->right_child_ = task;
    }
  }

  ++m_internal->tasksSize;

  manageAutoShutdownTimer();

  return task;
}

parac_task*
TaskStore::pop_offload(parac_compute_node* target, TaskChecker check) {
  {
    std::unique_lock lock(m_internal->containerMutex);
    if(m_internal->tasksWaitingForWorkerQueue.empty()) {
      return nullptr;
    }

    // Reverse, so that shorter tasks are offloaded first.
    for(auto it = m_internal->tasksWaitingForWorkerQueue.rbegin();
        it != m_internal->tasksWaitingForWorkerQueue.rend();
        ++it) {
      parac_task& t = *it;

      if(!t.serialize)
        continue;

      if(check && !check(t))
        continue;

      // Found a suitable task to offload!

      // Erase the entry! Goes back to how to erase using a reverse_iterator:
      // https://stackoverflow.com/a/1830240
      m_internal->tasksWaitingForWorkerQueue.erase((++it).base());

      t.offloaded_to = target;

      auto rep = t.path.rep;
      m_internal->offloadedTasks[target].insert({ rep, t });
      t.state =
        static_cast<parac_task_state>(t.state & ~PARAC_TASK_WORK_AVAILABLE);
      t.state = t.state | PARAC_TASK_OFFLOADED;

      Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
        reinterpret_cast<std::byte*>(&t) - offsetof(Internal::Task, t));
      ++taskWrapper->refcount;
      --m_internal->tasksWaitingForWorkerQueueSize;

      decrementWorkQueueInComputeNode(m_internal->handle, t.originator);
      return &t;
    }
  }
  return nullptr;
}
parac_task*
TaskStore::pop_work() {
  parac_task* t = nullptr;
  {
    std::unique_lock lock(m_internal->containerMutex);
    if(m_internal->tasksWaitingForWorkerQueue.empty()) {
      return nullptr;
    }
    t = &m_internal->tasksWaitingForWorkerQueue.front().get();
    m_internal->tasksWaitingForWorkerQueue.pop_front();
    m_internal->tasksBeingWorkedOn.insert(*t);
    t->state =
      static_cast<parac_task_state>(t->state & ~PARAC_TASK_WORK_AVAILABLE);
    t->state = t->state | PARAC_TASK_WORKING;

    Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(t) - offsetof(Internal::Task, t));
    ++taskWrapper->refcount;
    --m_internal->tasksWaitingForWorkerQueueSize;
  }

  decrementWorkQueueInComputeNode(m_internal->handle, t->originator);

  return t;
}
void
TaskStore::undo_offload(parac_task* t) {
  {
    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Undo offload of task on path {} from {} to this node.",
              t->path,
              t->offloaded_to->id);

    std::unique_lock lock(m_internal->containerMutex);

    m_internal->offloadedTasks[t->offloaded_to].erase(t->path.rep);
    t->state = static_cast<parac_task_state>(t->state & ~PARAC_TASK_OFFLOADED);

    Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(t) - offsetof(Internal::Task, t));
    --taskWrapper->refcount;

    t->offloaded_to = nullptr;
  }

  insert_into_tasksWaitingForWorkerQueue(t);
}

void
TaskStore::insert_into_tasksWaitingForWorkerQueue(parac_task* task) {
  assert(task);

  task->state = task->state | PARAC_TASK_WORK_AVAILABLE;

  if(task->received_from) {
    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Receive and insert task on path {} with originator {} from "
              "remote node {}.",
              task->path,
              task->originator,
              task->received_from->id);
  }

  {
    std::unique_lock lock(m_internal->containerMutex);
    m_internal->tasksWaitingForWorkerQueue.insert(
      std::lower_bound(m_internal->tasksWaitingForWorkerQueue.begin(),
                       m_internal->tasksWaitingForWorkerQueue.end(),
                       task->path,
                       [](const Internal::TaskRef& t, parac_path p) {
                         return t.get().path.length > p.length;
                       }),
      *task);

    ++m_internal->tasksWaitingForWorkerQueueSize;

    incrementWorkQueueInComputeNode(m_internal->handle, task->originator);

    if(m_internal->store.ping_on_work) {
      m_internal->store.ping_on_work(&m_internal->store);
    }
  }
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
TaskStore::insert_into_tasksBeingWorkedOn(parac_task* task) {
  assert(task);
  std::unique_lock lock(m_internal->containerMutex);
  m_internal->tasksBeingWorkedOn.insert(*task);
}
void
TaskStore::remove_from_tasksBeingWorkedOn(parac_task* task) {
  assert(task);
  std::unique_lock lock(m_internal->containerMutex);
  m_internal->tasksBeingWorkedOn.erase(*task);
}

void
TaskStore::remove(parac_task* task) {
  Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
    reinterpret_cast<std::byte*>(task) - offsetof(Internal::Task, t));
  assert(taskWrapper);

  std::unique_lock lock(m_internal->containerMutex);

  --taskWrapper->refcount;

  if(taskWrapper->refcount > 0)
    return;

  assert(task);
  assert(!(task->state & PARAC_TASK_WAITING_FOR_SPLITS));
  assert(!(task->state & PARAC_TASK_WORK_AVAILABLE));
  assert(task->state & PARAC_TASK_DONE);

  // Remove references to now removed path.
  if(task->left_child_)
    task->left_child_->parent_task_ = nullptr;
  if(task->right_child_)
    task->right_child_->parent_task_ = nullptr;

  if(task->parent_task_) {
    Internal::Task* parentWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(task->parent_task_) -
      offsetof(Internal::Task, t));
    --parentWrapper->refcount;

    if(task->parent_task_->left_child_ == task)
      task->parent_task_->left_child_ = nullptr;
    if(task->parent_task_->right_child_ == task)
      task->parent_task_->right_child_ = nullptr;
  }

  --m_internal->tasksSize;

  manageAutoShutdownTimer();

  m_internal->tasks.erase(taskWrapper->it);
}

void
TaskStore::autoShutdownTimerExpired(parac_timeout* t) {
  assert(t);
  assert(t->expired_userdata);
  TaskStore* store = static_cast<TaskStore*>(t->expired_userdata);
  parac_handle& handle = store->m_internal->handle;
  parac_log(PARAC_BROKER,
            PARAC_INFO,
            "Automatic Shutdown Timer (set to {}ms of inactivity, meaning no "
            "tasks to work on) expired!",
            store->m_autoShutdownTimeout);
  handle.exit_status = PARAC_AUTO_SHUTDOWN_TRIGGERED;
  handle.request_exit(&handle);
}

void
TaskStore::manageAutoShutdownTimer() {
  /// The container mutex MUST be locked at this point!

  if(m_autoShutdownTimeout == 0)
    return;

  auto mod_comm = m_internal->handle.modules[PARAC_MOD_COMMUNICATOR];
  assert(mod_comm);
  parac_module_communicator* comm = mod_comm->communicator;

  if(m_internal->tasksSize == 0) {
    // No tasks are left in the broker, so trigger a new timeout for auto
    // shutdown.
    m_internal->autoShutdownTimeout =
      comm->set_timeout(mod_comm,
                        m_autoShutdownTimeout,
                        this,
                        &TaskStore::autoShutdownTimerExpired);
  } else if(m_internal->autoShutdownTimeout) {
    m_internal->autoShutdownTimeout->cancel(m_internal->autoShutdownTimeout);
    m_internal->autoShutdownTimeout = nullptr;
  }
}

void
TaskStore::assess_task(parac_task* task) {
  assert(task);
  assert(task->assess);
  assert(task->path.rep != PARAC_PATH_EXPLICITLY_UNKNOWN);

  // Required so that no concurrent assessments of the same tasks are happening.
  std::unique_lock lock(m_internal->assessMutex);

  parac_task_state s = task->assess(task);
  task->state = s;

  parac_log(PARAC_BROKER,
            PARAC_TRACE,
            "Assessing task on path {} to state {}. Work available in store: "
            "{}, tasks in store: {}",
            task->path,
            task->state,
            m_internal->tasksWaitingForWorkerQueueSize,
            m_internal->tasksSize);

  if(parac_task_state_is_done(s)) {
    assert(!(s & PARAC_TASK_WORK_AVAILABLE));

    remove_from_tasksBeingWorkedOn(task);

    if(m_internal->handle.input_file && parac_path_is_root(task->path)) {
      m_internal->handle.request_exit(&m_internal->handle);
      m_internal->handle.exit_status = task->result;
    }

    if((s & PARAC_TASK_SPLITS_DONE) == PARAC_TASK_SPLITS_DONE) {
      remove_from_tasksWaitingForChildren(task);
    }

    auto path = task->path;
    auto result = task->result;
    auto originator = task->originator;
    parac_task* parent = task->parent_task_;

    remove(task);

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Task on path {} with originator {} done with result {}",
              path,
              originator,
              result);

    if(parent) {
      parac_path parentPath = parent->path;

      if(path.rep == parac_path_get_next_left(parentPath).rep) {
        parent->left_result = result;
      } else if(path.rep == parac_path_get_next_right(parentPath).rep) {
        parent->right_result = result;
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

    return;
  }

  // Must be at the end, because priority inversion could finish the task and
  // immediately remove it, leading to double invocations and chaos!
  if(s & PARAC_TASK_WORK_AVAILABLE && !(s & PARAC_TASK_DONE)) {
    insert_into_tasksWaitingForWorkerQueue(task);
  }
  if(!(task->last_state & PARAC_TASK_WAITING_FOR_SPLITS) &&
     s & PARAC_TASK_WAITING_FOR_SPLITS) {
    insert_into_tasksWaitingForChildren(task);
  }

  task->last_state = s;
}

parac_task_store&
TaskStore::store() {
  return m_internal->store;
}

void
TaskStore::receiveTaskResultFromPeer(parac_message& msg) {
  auto originId = msg.origin->id;
  auto path = parac_task_result_packet_get_path(msg.data);
  parac_status result = parac_task_result_packet_get_result(msg.data);

  auto pathTaskMapIt = m_internal->offloadedTasks.find(msg.origin);
  if(pathTaskMapIt == m_internal->offloadedTasks.end()) {
    parac_log(
      PARAC_BROKER,
      PARAC_GLOBALERROR,
      "Task result {} on path {} with originator {} received from {}, but node "
      "not found in broker task store! Ignoring.",
      result,
      path,
      msg.originator_id,
      originId);
    msg.cb(&msg, PARAC_COMPUTE_NODE_NOT_FOUND_ERROR);
    return;
  }

  auto& pathTaskMap = pathTaskMapIt->second;

  auto taskIt = pathTaskMap.find(path.rep);
  if(taskIt == pathTaskMap.end()) {
    parac_log(
      PARAC_BROKER,
      PARAC_GLOBALERROR,
      "Task result {} on path {} with originator {} received from {}, but "
      "task path "
      "not found in broker task store node map! Ignoring.",
      result,
      path,
      msg.originator_id,
      originId);
    msg.cb(&msg, PARAC_PATH_NOT_FOUND_ERROR);
    return;
  }

  auto& task = taskIt->second.get();
  task.result = result;
  task.state = task.state | PARAC_TASK_DONE;

  assess_task(&task);

  msg.cb(&msg, PARAC_OK);
}
}
