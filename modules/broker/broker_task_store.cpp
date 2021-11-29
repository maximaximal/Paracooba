#include <algorithm>
#include <atomic>
#include <cassert>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

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

// The memory pool optimizes performance but may cause issues while debugging,
// as the same memory is re-used for multiple tasks.

#define USE_MEMORY_POOL

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
  std::recursive_mutex containerMutex;
  parac_handle& handle;
  parac_task_store& store;

  struct Task;
#ifdef USE_MEMORY_POOL
  template<typename T>
  using Allocator =
    boost::fast_pool_allocator<T,
                               boost::default_user_allocator_new_delete,
                               boost::details::pool::default_mutex,
                               64>;
  using TaskRef = std::reference_wrapper<parac_task>;
  using TaskList = std::list<Task, Allocator<Task>>;
  using TaskRefList = std::list<TaskRef, Allocator<TaskRef>>;
#else
  using TaskRef = std::reference_wrapper<parac_task>;
  using TaskList = std::list<Task>;
  using TaskRefList = std::list<TaskRef>;
#endif

  struct Task {
    TaskList::iterator it;
    TaskRefList::iterator workQueueIt;
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
      if(t.delete_notification) {
        *t.delete_notification = true;
      }
    }
  };

  struct TaskRefCompare {
    bool operator()(const TaskRef& lhs, const TaskRef& rhs) const {
      return static_cast<parac::Path&>(lhs.get().path) >
             static_cast<parac::Path&>(rhs.get().path);
    }
  };

  TaskList tasks;

  // std::list<TaskRef, Allocator<TaskRef>> tasksWaitingForWorkerQueue;
  // using TaskSet = std::set<TaskRef, TaskRefCompare, Allocator<TaskRef>>;
  std::list<TaskRef> tasksWaitingForWorkerQueue;
  using TaskSet = std::set<TaskRef, TaskRefCompare>;

  TaskSet tasksBeingWorkedOn;
  TaskSet tasksWaitingForChildren;

#ifdef USE_MEMORY_POOL
  std::unordered_map<parac_compute_node*,
                     std::unordered_set<Task*,
                                        std::hash<Task*>,
                                        std::equal_to<Task*>,
                                        Allocator<Task*>>>
    offloadedTasks;
#else
  std::unordered_map<
    parac_compute_node*,
    std::unordered_set<Task*, std::hash<Task*>, std::equal_to<Task*>>>
    offloadedTasks;
#endif

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
  s.pop_work = [](parac_task_store* store, volatile bool* delete_notification) {
    assert(store);
    assert(store->userdata);
    return static_cast<TaskStore*>(store->userdata)
      ->pop_work(delete_notification);
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

void
TaskStore::default_terminate_task(volatile parac_task* t) {
  assert(t);
  assert(t->task_store);
  assert(t->task_store->userdata);

  TaskStore* s = static_cast<TaskStore*>(t->task_store->userdata);

  parac_path p = ((parac_task*)t)->path;

  parac_log(PARAC_BROKER, PARAC_TRACE, "Default terminate task on path {}", p);

  if(t->state & PARAC_TASK_OFFLOADED) {
    assert(t->offloaded_to);
    parac_message_wrapper msg;
    msg.kind = PARAC_MESSAGE_TASK_ABORT;
    msg.originator_id = t->originator;
    msg.data_is_inline = true;
    msg.data_to_be_freed = false;
    uintptr_t* ptr = reinterpret_cast<uintptr_t*>(msg.inline_data);
    *ptr = reinterpret_cast<uintptr_t>(static_cast<volatile void*>(t));
    t->offloaded_to->send_message_to(t->offloaded_to, &msg);

    // No callback required, the task is just aborted.
    t->state = PARAC_TASK_DONE;
    s->assess_task((parac_task*)t);
    return;
  }

  t->state = PARAC_TASK_DONE;
  t->result = PARAC_ABORTED;
  t->work = nullptr;
  t->stop = true;

  if(t->state & PARAC_TASK_WORKING) {
    return;
  }

  Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
    reinterpret_cast<std::byte*>((void*)t) - offsetof(Internal::Task, t));
  // Delete only after final assessment. The reference in this function needs to
  // be counted too.
  ++taskWrapper->refcount;

  auto left_result = t->left_result;
  auto left_child = t->left_child_;

  auto right_result = t->right_result;
  auto right_child = t->right_child_;

  if(t->left_child_) {
    t->left_child_->parent_task_ = nullptr;
    t->left_child_ = nullptr;
  }
  if(t->right_child_) {
    t->right_child_->parent_task_ = nullptr;
    t->right_child_ = nullptr;
  }

  if(taskWrapper->workQueueIt !=
     s->m_internal->tasksWaitingForWorkerQueue.end())
    s->remove_from_workQueue((parac_task*)t);
  s->remove_from_tasksBeingWorkedOn((parac_task*)t);

  if(left_result == PARAC_PENDING && left_child && left_child->terminate) {
    left_child->parent_task_ = nullptr;
    left_child->terminate(left_child);
  }

  if(right_result == PARAC_PENDING && right_child && right_child->terminate) {
    right_child->parent_task_ = nullptr;
    right_child->terminate(right_child);
  }

  s->assess_task((parac_task*)t, false, false);
}

parac_task*
TaskStore::newTask(parac_task* parent_task,
                   parac_path new_path,
                   parac_id originator) {
  std::unique_lock lock(m_internal->containerMutex);
  auto& taskWrapper = m_internal->tasks.emplace_front();
  parac_task* task = &taskWrapper.t;
  taskWrapper.it = m_internal->tasks.begin();
  taskWrapper.workQueueIt = m_internal->tasksWaitingForWorkerQueue.end();
  parac_task_init(task);
  task->parent_task_ = parent_task;
  task->task_store = &m_internal->store;
  task->path = new_path;
  task->originator = originator;
  task->handle = &m_internal->handle;

  if(parent_task) {
    Internal::Task* parentWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(parent_task) - offsetof(Internal::Task, t));
    ++(parentWrapper->refcount);

    // Extended tasks don't have left or right children! Responsibility to
    // assign a path->extended_children array is with the caller.
    if(!parac_path_is_extended(new_path)) {
      if(parac_path_get_next_left(parent_task->path).rep == new_path.rep) {
        parent_task->left_child_ = task;
      } else if(parac_path_get_next_right(parent_task->path).rep ==
                new_path.rep) {
        parent_task->right_child_ = task;
      }
      assert(parent_task->left_child_ || parent_task->right_child_);
    }
    if(parent_task->stop) {
      return nullptr;
    }
  }

  ++m_internal->tasksSize;

  manageAutoShutdownTimer();

  task->terminate = default_terminate_task;

  return task;
}

parac_task*
TaskStore::pop_offload(parac_compute_node* target, TaskChecker check) {
  parac_task* returned = nullptr;
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

      if(t.received_from)
        continue;

      if(check && !check(t))
        continue;

      // Found a suitable task to offload!

      // Erase the entry! Goes back to how to erase using a reverse_iterator:
      // https://stackoverflow.com/a/1830240
      m_internal->tasksWaitingForWorkerQueue.erase((++it).base());

      t.offloaded_to = target;

      Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
        reinterpret_cast<std::byte*>(&t) - offsetof(Internal::Task, t));

      t.state =
        static_cast<parac_task_state>(t.state & ~PARAC_TASK_WORK_AVAILABLE);
      t.state = t.state | PARAC_TASK_OFFLOADED;

      m_internal->offloadedTasks[target].emplace(taskWrapper);

      ++taskWrapper->refcount;
      taskWrapper->workQueueIt = m_internal->tasksWaitingForWorkerQueue.end();
      --m_internal->tasksWaitingForWorkerQueueSize;

      returned = &t;
      break;
    }
  }
  if(returned)
    decrementWorkQueueInComputeNode(m_internal->handle, returned->originator);
  return returned;
}
parac_task*
TaskStore::pop_work(volatile bool* delete_notification) {
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
    t->delete_notification = delete_notification;

    Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(t) - offsetof(Internal::Task, t));
    ++taskWrapper->refcount;
    taskWrapper->workQueueIt = m_internal->tasksWaitingForWorkerQueue.end();
    --m_internal->tasksWaitingForWorkerQueueSize;
  }

  return t;
}
void
TaskStore::undo_offload(parac_task* t) {
  assert(t->offloaded_to);
  {
    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Undo offload of task on path {} from {} to this node.",
              t->path,
              t->offloaded_to->id);

    std::unique_lock lock(m_internal->containerMutex);

    Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>(t) - offsetof(Internal::Task, t));

    auto& offloaded_to = m_internal->offloadedTasks[t->offloaded_to];

    auto it = offloaded_to.find(taskWrapper);
    if(it == offloaded_to.end()) {
      parac_log(PARAC_BROKER,
                PARAC_LOCALERROR,
                "Tried to undo offload of task with wrapper address {} and "
                "path {} that was not found in offloaded_to {}!",
                t->path,
                t->path,
                t->offloaded_to->id);
      return;
    }
    offloaded_to.erase(it);

    t->state = static_cast<parac_task_state>(t->state & ~PARAC_TASK_OFFLOADED);
    t->offloaded_to = nullptr;

    --taskWrapper->refcount;
  }

  insert_into_tasksWaitingForWorkerQueue(t);
}

void
TaskStore::undoAllOffloadsTo(parac_compute_node* remote) {
  assert(remote);
  parac_log(
    PARAC_BROKER, PARAC_TRACE, "Undo all offloads to remote {}.", remote->id);

  std::unique_lock lock(m_internal->containerMutex);

  auto& offloaded_to = m_internal->offloadedTasks[remote];
  for(auto it = offloaded_to.begin(); it != offloaded_to.end();) {
    Internal::Task* taskWrapper = *it;
    parac_task* t = &taskWrapper->t;

    assert(t->offloaded_to);

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Undo all offloads to remote {}, so undoing offload of task {}",
              remote->id,
              t->path);

    it = offloaded_to.erase(it);

    t->state = static_cast<parac_task_state>(t->state & ~PARAC_TASK_OFFLOADED);
    t->offloaded_to = nullptr;

    --taskWrapper->refcount;

    insert_into_tasksWaitingForWorkerQueue(t);
  }
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
    {
      std::unique_lock lock(m_internal->containerMutex);
      auto it = m_internal->tasksWaitingForWorkerQueue.insert(
        std::lower_bound(m_internal->tasksWaitingForWorkerQueue.begin(),
                         m_internal->tasksWaitingForWorkerQueue.end(),
                         task,
                         [](const Internal::TaskRef& l, parac_task* r) {
                           return parac_task_compare(&l.get(), r);
                         }),
        *task);
      Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
        reinterpret_cast<std::byte*>(task) - offsetof(Internal::Task, t));
      taskWrapper->workQueueIt = it;
    }

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
TaskStore::remove_from_workQueue(parac_task* task) {
  std::unique_lock lock(m_internal->containerMutex);
  Internal::Task* taskWrapper = reinterpret_cast<Internal::Task*>(
    reinterpret_cast<std::byte*>(task) - offsetof(Internal::Task, t));
  if(taskWrapper->workQueueIt != m_internal->tasksWaitingForWorkerQueue.end()) {
    --m_internal->tasksWaitingForWorkerQueueSize;
    decrementWorkQueueInComputeNode(m_internal->handle, task->originator);
    m_internal->tasksWaitingForWorkerQueue.erase(taskWrapper->workQueueIt);
    taskWrapper->workQueueIt = m_internal->tasksWaitingForWorkerQueue.end();
  }
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
  std::unique_lock assessLock(m_internal->assessMutex);

  --taskWrapper->refcount;

  // if(taskWrapper->refcount > 0)
  //  return;

  assert(task);
  assert(task->result == PARAC_ABORTED ||
         !(task->state & PARAC_TASK_WAITING_FOR_SPLITS));
  assert(task->result == PARAC_ABORTED ||
         !(task->state & PARAC_TASK_WORK_AVAILABLE));
  assert(!(task->state & PARAC_TASK_WORKING));
  assert(task->state & PARAC_TASK_DONE);

  // Remove references to now removed path.
  if(task->left_child_)
    task->left_child_->parent_task_ = nullptr;
  if(task->right_child_)
    task->right_child_->parent_task_ = nullptr;

  bool hadleftchild = task->left_child_;
  bool hadrightchild = task->right_child_;

  parac_log(PARAC_BROKER,
            PARAC_TRACE,
            "Remove task on path {} with address {}. Had left child: {}, had "
            "right child: {}. Had parent task: {}",
            task->path,
            static_cast<void*>(task),
            hadleftchild,
            hadrightchild,
            static_cast<void*>((parac_task*)task->parent_task_));

  if(task->parent_task_ && !task->received_from) {
    Internal::Task* parentWrapper = reinterpret_cast<Internal::Task*>(
      reinterpret_cast<std::byte*>((void*)task->parent_task_) -
      offsetof(Internal::Task, t));
    --parentWrapper->refcount;

    assert(!(task->parent_task_->left_child_ == task &&
             task->parent_task_->right_child_ == task));

    if(task->parent_task_->left_child_ == task) {
      task->parent_task_->left_child_ = nullptr;
    }
    if(task->parent_task_->right_child_ == task) {
      task->parent_task_->right_child_ = nullptr;
    }
  }

  --m_internal->tasksSize;

  manageAutoShutdownTimer();

  m_internal->tasks.erase(taskWrapper->it);
}

void
TaskStore::abort_tasks_with_parent_and_originator(parac_task* parent,
                                                  parac_id originator) {
  Internal::TaskRefList list;

  {
    std::unique_lock lock(m_internal->containerMutex);
    for(auto& t : m_internal->tasks) {
      if(t.t.originator == originator && t.t.parent_task_ == parent) {
        list.emplace_front(t.t);
      }
    }
  }

  for(const auto& tw : list) {
    auto& t = tw.get();
    t.terminate(&t);
  }
}

void
TaskStore::abort_tasks_received_from(parac_compute_node* remote) {
  using It = Internal::TaskList::iterator;
  It it = m_internal->tasks.begin();

  auto next = [remote, this](It& it) -> bool {
    std::unique_lock lock(m_internal->containerMutex);
    while(it != m_internal->tasks.end() && it->t.received_from != remote) {
      ++it;
    }
    return it != m_internal->tasks.end();
  };

  while(next(it)) {
    auto& t = it->t;
    ++it;
    t.terminate(&t);
  }
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
TaskStore::terminateAllTasks() {
  parac_log(PARAC_BROKER, PARAC_TRACE, "Terminate all running tasks!");
  std::unique_lock lock(m_internal->containerMutex);
  for(auto& t : m_internal->tasksWaitingForWorkerQueue) {
    t.get().stop = true;
  }
  for(auto t : m_internal->tasksBeingWorkedOn) {
    auto& task = t.get();
    // It is only relevant as long as the termination function is not the
    // default one, as the default tasks are not running anyways.
    if(task.terminate && task.terminate != TaskStore::default_terminate_task) {
      task.terminate(&task);
    }
  }
}

void
TaskStore::assess_task(parac_task* task, bool remove, bool removeParent) {
  assert(task);

  // Required so that no concurrent assessments of the same tasks are happening.
  std::unique_lock lock(m_internal->assessMutex);

  assert(task->assess);
  assert(task->path.rep != PARAC_PATH_EXPLICITLY_UNKNOWN);

  parac_task_state s = task->assess(task);
  task->state = s;

  parac_log(PARAC_BROKER,
            PARAC_TRACE,
            "Assessing task on path {} (pre-psc: {}, post-psc: {}) to state "
            "{}. Current result {}. Work "
            "available in store: "
            "{}, tasks in store: {}",
            task->path,
            task->pre_path_sorting_critereon,
            task->post_path_sorting_critereon,
            static_cast<parac_task_state>(task->state),
            static_cast<parac_status>(task->result),
            m_internal->tasksWaitingForWorkerQueueSize,
            m_internal->tasksSize);

  if(parac_task_state_is_done(s)) {
    if(!(task->state & PARAC_TASK_OFFLOADED)) {
      decrementWorkQueueInComputeNode(m_internal->handle, task->originator);
    }

    assert(!(s & PARAC_TASK_WORK_AVAILABLE));

    if(task->path.rep == PARAC_PATH_PARSER) {
      auto& computeNodeStore = extractComputeNodeStore(m_internal->handle);
      computeNodeStore.formulaParsed(task->originator);
    }

    remove_from_tasksBeingWorkedOn(task);

    if(m_internal->handle.input_file && parac_path_is_root(task->path) &&
       (task->result != PARAC_ABORTED && task->result != PARAC_UNDEFINED &&
        m_internal->handle.exit_status != PARAC_SAT &&
        m_internal->handle.exit_status != PARAC_UNSAT) &&
       m_internal->handle.exit_status == PARAC_UNDEFINED) {
      m_internal->handle.exit_status = task->result;
      m_internal->handle.request_exit(&m_internal->handle);
    }

    if((s & PARAC_TASK_SPLITS_DONE) == PARAC_TASK_SPLITS_DONE) {
      remove_from_tasksWaitingForChildren(task);
    }

    auto path = task->path;
    auto result = task->result;
    auto originator = task->originator;
    parac_task* parent = (parac_task*)task->parent_task_;

    if(task->received_from)
      parent = nullptr;

    if(remove)
      TaskStore::remove(task);

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Task on path {} with originator {} done with result {}",
              path,
              originator,
              result);

    // A task cannot be both done and splitted.
    assert(result != PARAC_SPLITTED);
    // A done task must have been aborted in order for the task result not being
    // either SAT or UNSAT.
    assert(result != PARAC_UNKNOWN);

    if(parent) {
      parac_path parentPath = parent->path;

      if(!parac_path_is_extended(path)) {
        assert(path.rep == parac_path_get_next_left(parentPath).rep ||
               path.rep == parac_path_get_next_right(parentPath).rep);

        if(path.rep == parac_path_get_next_left(parentPath).rep) {
          parent->left_result = result;
        } else if(path.rep == parac_path_get_next_right(parentPath).rep) {
          parent->right_result = result;
        } else {
          parac_log(
            PARAC_BROKER,
            PARAC_GLOBALERROR,
            "Task on path {} specified parent path on path {}, but is no "
            "direct child of parent! Ignoring parent.",
            path,
            parentPath);
          return;
        }
      }

      assess_task(parent, removeParent, removeParent);
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
  parac_task* t = parac_task_result_packet_get_task_ptr(msg.data);
  Internal::Task* tWrapper = reinterpret_cast<Internal::Task*>(
    reinterpret_cast<std::byte*>(t) - offsetof(Internal::Task, t));

  assert(t);
  assert(tWrapper);

  parac_status result = parac_task_result_packet_get_result(msg.data);

  auto pathTaskMapIt = m_internal->offloadedTasks.find(msg.origin);
  if(pathTaskMapIt == m_internal->offloadedTasks.end()) {
    parac_log(
      PARAC_BROKER,
      PARAC_GLOBALERROR,
      "Task result {} with originator {} and ptr {} received from {}, but node "
      "not found in broker task store! Ignoring.",
      result,
      msg.originator_id,
      static_cast<void*>(t),
      originId);
    msg.cb(&msg, PARAC_COMPUTE_NODE_NOT_FOUND_ERROR);
    return;
  }

  auto& pathTaskMap = pathTaskMapIt->second;

  auto taskIt = pathTaskMap.find(tWrapper);
  if(taskIt == pathTaskMap.end()) {
    parac_log(
      PARAC_BROKER,
      PARAC_GLOBALERROR,
      "Task result {} with originator {} and ptr {} received from {}, but "
      "task path "
      "not found in broker task store node map! Ignoring.",
      result,
      msg.originator_id,
      static_cast<void*>(t),
      originId);
    msg.cb(&msg, PARAC_PATH_NOT_FOUND_ERROR);
    return;
  }

  t->result = result;
  t->state = t->state | PARAC_TASK_DONE;

  // No longer offloaded!
  --tWrapper->refcount;
  pathTaskMap.erase(taskIt);

  assess_task(t);

  msg.cb(&msg, PARAC_OK);
}
}
