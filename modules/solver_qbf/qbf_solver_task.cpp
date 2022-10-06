#include <cassert>

#include <elf.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>
#include <paracooba/common/status.h>
#include <paracooba/common/task_store.h>

#include "paracooba/common/path.h"
#include "parser_qbf.hpp"
#include "qbf_solver_manager.hpp"
#include "qbf_solver_task.hpp"
#include "solver_qbf_config.hpp"

namespace parac::solver_qbf {
static parac_task_assess_func
GetAssessFunc(Parser::Quantifier qu) {
  switch(qu.type()) {
    case Parser::Quantifier::EXISTENTIAL:
      return parac_task_qbf_existential_assess;
    case Parser::Quantifier::UNIVERSAL:
      return parac_task_qbf_universal_assess;
  }
  return nullptr;
}

QBFSolverTask::QBFSolverTask(parac_handle& handle,
                             parac_task& task,
                             QBFSolverManager& manager,
                             std::shared_ptr<cubesource::Source> cubeSource)
  : m_handle(handle)
  , m_task(task)
  , m_manager(manager)
  , m_cubeSource(std::move(cubeSource)) {
  Parser::Quantifier qu{ 0 };
  const size_t cubeSize = m_cubeSource->cubeSize();

  if(manager.parser().quantifiers().size() > cubeSize) {
    qu = manager.parser().quantifiers()[cubeSize];
  } else {
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "QBF Solver reached limit of quantifier prefix! Task on path {} "
              "has longer cube "
              "size ({}+1) than the quantifier prefix ({})!",
              task.path,
              cubeSize,
              manager.parser().quantifiers().size());
  }

  m_task.assess = GetAssessFunc(qu);
  m_task.work = static_work;
  m_task.free_userdata = static_free_userdata;
  m_task.serialize = static_serialize;
  m_task.userdata = this;

  task.state = PARAC_TASK_WORK_AVAILABLE;
}
QBFSolverTask::~QBFSolverTask() {
  std::unique_lock lock(m_terminationMutex);
  if(m_cleanupSelfPointer)
    *m_cleanupSelfPointer = nullptr;
}

parac_status
QBFSolverTask::work(parac_worker worker) {
  if(m_task.stop)
    return PARAC_ABORTED;

  if(m_manager.parser().quantifiers().size() == 0) {
    if(m_manager.parser().isTrivial()) {
      if(m_manager.parser().isTrivialSAT()) {
        return PARAC_SAT;
      }
      if(m_manager.parser().isTrivialUNSAT()) {
        return PARAC_UNSAT;
      }
    }
  }

  struct Cleanup {
    explicit Cleanup(QBFSolverTask& t,
                     QBFSolverManager& manager,
                     parac_worker worker)
      : t(&t)
      , handle(manager.get(worker)) {
      termfunc = t.m_task.terminate;
      t.m_task.terminate = static_terminate;
      t.m_terminationFunc = [this]() { handle->terminate(); };
      t.m_cleanupSelfPointer = &this->t;
    }
    ~Cleanup() {
      if(this->t) {
        QBFSolverTask& t = *((QBFSolverTask*)this->t);
        std::unique_lock lock(t.m_terminationMutex);

        t.m_terminationFunc = nullptr;
        t.m_task.terminate = termfunc;
        t.m_cleanupSelfPointer = nullptr;
      }
    }

    volatile QBFSolverTask* t;
    parac_task_terminate_func termfunc;
    QBFSolverManager::PtrWrapper handle;
  };
  Cleanup cleanup{ *this, m_manager, worker };
  auto& handle = cleanup.handle;

  bool split_left = false, split_right = false, split_extended = false;
  if(m_cubeSource->split(m_task.path,
                         m_manager,
                         *handle,
                         split_left,
                         split_right,
                         split_extended) &&
     (m_manager.config().treeDepth() == -1 ||
      static_cast<parac_path_length_type>(m_manager.config().treeDepth()) >
        parac_path_length(m_task.path))) {
    // The split may already be known from the cube source (think of predefined
    // cubes). In this case, just split the task and create new tasks.

    m_task.state =
      m_task.state | PARAC_TASK_SPLITTED | PARAC_TASK_WAITING_FOR_SPLITS;

    if(split_left) {
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Task on path {} lsplit to path {}",
                m_task.path,
                parac_path_get_next_left(m_task.path));
      assert(m_task.task_store);
      parac_task* l =
        m_task.task_store->new_task(m_task.task_store,
                                    &m_task,
                                    parac_path_get_next_left(m_task.path),
                                    m_task.originator);

      m_task.left_result = PARAC_PENDING;

      if(l) {
        new QBFSolverTask(
          m_handle, *l, m_manager, m_cubeSource->leftChild(m_cubeSource));
        if(l->result != PARAC_ABORTED && m_task.result != PARAC_ABORTED)
          m_task.task_store->assess_task(m_task.task_store, l);
      }
    } else if(!split_extended) {
      m_task.left_result = PARAC_UNSAT;
    }

    if(split_right) {
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Task on path {} rsplit to path {}",
                m_task.path,
                parac_path_get_next_right(m_task.path));
      assert(m_task.task_store);
      parac_task* r =
        m_task.task_store->new_task(m_task.task_store,
                                    &m_task,
                                    parac_path_get_next_right(m_task.path),
                                    m_task.originator);

      m_task.right_result = PARAC_PENDING;

      if(r) {
        new QBFSolverTask(
          m_handle, *r, m_manager, m_cubeSource->rightChild(m_cubeSource));
        if(r->result != PARAC_ABORTED && m_task.result != PARAC_ABORTED) {
          m_task.task_store->assess_task(m_task.task_store, r);
        }
      }
    } else if(!split_extended) {
      m_task.right_result = PARAC_UNSAT;
    }

    if(split_extended) {
      assert(!split_left);
      assert(!split_right);

      auto sources = m_cubeSource->children(m_cubeSource);
      m_extended_subtasks_arr.resize(sources.size());
      m_extended_subtasks_results_arr.resize(sources.size());
      m_task.extended_children = m_extended_subtasks_arr.data();
      m_task.extended_children_results = m_extended_subtasks_results_arr.data();
      m_task.extended_children_count = sources.size();
      for(size_t i = 0; i < sources.size(); ++i) {
        assert(m_task.task_store);
        parac_task* t =
          m_task.task_store->new_task(m_task.task_store,
                                      &m_task,
                                      parac_path_get_next_extended(m_task.path),
                                      m_task.originator);

        if(parac_log_enabled(PARAC_SOLVER, PARAC_TRACE)) {
          auto cube = sources[i]->cube(t->path, m_manager);
          parac_log(PARAC_SOLVER,
                    PARAC_TRACE,
                    "Task on path {} split to path next {} with cube {}",
                    m_task.path,
                    parac_path_get_next_extended(m_task.path),
                    cube);
        }

        t->extended_children_parent_index = i;
        m_task.extended_children[i] = t;
        m_task.extended_children_results[i] = PARAC_PENDING;
        assert(t);
        new QBFSolverTask(m_handle, *t, m_manager, sources[i]);
      }
      // Start them all at once when the tasks are already in the array!
      for(size_t i = 0; i < sources.size(); ++i) {
        parac_task* t = m_task.extended_children[i];
        if(t->result != PARAC_ABORTED && m_task.result != PARAC_ABORTED) {
          m_task.task_store->assess_task(m_task.task_store, t);
        } else {
          m_task.extended_children_results[i] = PARAC_ABORTED;
          break;
        }
      }
    }

    return PARAC_SPLITTED;
  } else {
    auto c = m_cubeSource->cube(m_task.path, m_manager);
    handle->assumeCube(c);
    parac_log(PARAC_SOLVER,
              PARAC_TRACE,
              "Apply cube {} on path {} to solver!",
              c,
              m_task.path);
    if(m_task.stop)
      return PARAC_ABORTED;
    parac_status r = handle->solve();
    if(r == PARAC_ABORTED) {
      std::unique_lock lock(m_terminationMutex);
      cleanup.t = nullptr;
      m_cleanupSelfPointer = nullptr;
      m_terminationFunc = nullptr;
    }
    return r;
  }
}
void
QBFSolverTask::terminate() {
  std::unique_lock<std::mutex> lock(m_terminationMutex, std::try_to_lock);
  if(!lock.owns_lock()) {
    return;
  }

  if(m_terminationFunc) {
    m_terminationFunc();
  }
}
parac_status
QBFSolverTask::serialize_to_msg(parac_message* tgt) {
  assert(tgt);

  if(!m_serializationOutStream) {
    m_serializationOutStream = std::make_unique<NoncopyOStringstream>();

    {
      cereal::BinaryOutputArchive oa(*m_serializationOutStream);
      parac_path_type p = m_task.path.rep;
      oa(p);
      oa(reinterpret_cast<intptr_t>(&m_task));
      oa(*this);
    }
  }

  tgt->kind = PARAC_MESSAGE_SOLVER_TASK;
  tgt->data = m_serializationOutStream->ptr();
  tgt->length = m_serializationOutStream->tellp();
  tgt->originator_id = m_task.originator;

  return PARAC_OK;
}
parac_status
QBFSolverTask::static_serialize(parac_task* task, parac_message* tgt) {
  assert(task);
  assert(task->userdata);
  QBFSolverTask* self = static_cast<QBFSolverTask*>(task->userdata);
  return self->serialize_to_msg(tgt);
}

parac_status
QBFSolverTask::static_work(parac_task* task, parac_worker worker) {
  assert(task);
  assert(task->userdata);
  QBFSolverTask* self = static_cast<QBFSolverTask*>(task->userdata);
  return self->work(worker);
}
void
QBFSolverTask::static_terminate(volatile parac_task* task) {
  assert(task);
  assert(task->userdata);
  QBFSolverTask* self = static_cast<QBFSolverTask*>(task->userdata);
  parac_path p;
  p.rep = task->path.rep;
  if(parac_path_is_extended(p) && task->extended_children) {
    for(int32_t i = 0; i < task->extended_children_count; ++i) {
      if(task->extended_children[i]) {
        task->extended_children[i]->parent_task_ = nullptr;
      }
    }
  } else {
    if(task->left_child_) {
      task->left_child_->parent_task_ = nullptr;
    }
    if(task->right_child_) {
      task->right_child_->parent_task_ = nullptr;
    }
  }

  return self->terminate();
}
parac_status
QBFSolverTask::static_free_userdata(parac_task* task) {
  assert(task);
  assert(task->userdata);
  QBFSolverTask* self = static_cast<QBFSolverTask*>(task->userdata);
  delete self;
  task->userdata = nullptr;
  task->free_userdata = nullptr;
  return PARAC_OK;
}
}
