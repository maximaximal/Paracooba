#include <cassert>

#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>
#include <paracooba/common/status.h>
#include <paracooba/common/task_store.h>

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
  if(manager.parser().quantifiers().size() > 0) {
    qu = manager.parser().quantifiers()[parac_path_length(task.path)];
  }

  m_task.assess = GetAssessFunc(qu);
  m_task.work = static_work;
  m_task.free_userdata = static_free_userdata;
  m_task.serialize = static_serialize;
  m_task.userdata = this;

  task.state = PARAC_TASK_WORK_AVAILABLE;
}
QBFSolverTask::~QBFSolverTask() {
  if(m_cleanupSelfPointer)
    *m_cleanupSelfPointer = nullptr;
}

parac_status
QBFSolverTask::work(parac_worker worker) {
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
    explicit Cleanup(QBFSolverTask& t, QBFSolverManager &manager, parac_worker worker)
      : t(&t), handle(manager.get(worker)) {
      termfunc = t.m_task.terminate;
      t.m_task.terminate = static_terminate;
      auto &handle = this->handle;
      t.m_terminationFunc = [&handle]() { handle->terminate(); };
      t.m_cleanupSelfPointer = &this->t;
    }
    ~Cleanup() {
      if(this->t) {
	QBFSolverTask &t = *((QBFSolverTask*) this->t);
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
  auto &handle = cleanup.handle;

  bool split_left = false, split_right = false;
  if(m_cubeSource->split(
       m_task.path, m_manager, *handle, split_left, split_right) &&
     (!m_manager.config().treeDepth() ||
      static_cast<parac_path_length_type>(m_manager.config().treeDepth()) >
        parac_path_length(m_task.path))) {
    // The split may already be known from the cube source (think of predefined
    // cubes). In this case, just split the task and create new tasks.

    m_task.state =
      PARAC_TASK_SPLITTED | PARAC_TASK_WAITING_FOR_SPLITS | PARAC_TASK_DONE;

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

      assert(l);
      new QBFSolverTask(
        m_handle, *l, m_manager, m_cubeSource->leftChild(m_cubeSource));
      m_task.left_result = PARAC_PENDING;
      if(l->result != PARAC_ABORTED && m_task.result != PARAC_ABORTED)
      m_task.task_store->assess_task(m_task.task_store, l);
    } else {
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

      assert(r);
      new QBFSolverTask(
        m_handle, *r, m_manager, m_cubeSource->rightChild(m_cubeSource));
      m_task.right_result = PARAC_PENDING;
      if(r->result != PARAC_ABORTED && m_task.result != PARAC_ABORTED)
        m_task.task_store->assess_task(m_task.task_store, r);
    } else {
      m_task.right_result = PARAC_UNSAT;
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
    return handle->solve();
  }
}
void
QBFSolverTask::terminate() {
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
QBFSolverTask::static_terminate(parac_task* task) {
  assert(task);
  assert(task->userdata);
  QBFSolverTask* self = static_cast<QBFSolverTask*>(task->userdata);
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
