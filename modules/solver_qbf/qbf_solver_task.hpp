#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include <paracooba/common/noncopy_ostream.hpp>
#include <paracooba/common/status.h>
#include <paracooba/common/task.h>

#include "qbf_cube_source.hpp"

struct parac_handle;
struct parac_task;

namespace parac::solver_qbf {
class QBFSolverManager;

class QBFSolverTask {
  public:
  QBFSolverTask(parac_handle& handle,
                parac_task& task,
                QBFSolverManager& manager,
                std::shared_ptr<cubesource::Source> cubeSource);
  ~QBFSolverTask();

  private:
  parac_handle& m_handle;
  parac_task& m_task;
  QBFSolverManager& m_manager;

  /// Only exists if the subtasks are extended, then this array is used to store
  /// the subtasks for the parac_task* array.
  std::vector<parac_task*> m_extended_subtasks_arr;
  std::vector<parac_status> m_extended_subtasks_results_arr;

  parac_status work(parac_worker worker);
  void terminate();
  parac_status serialize_to_msg(parac_message* tgt);

  static parac_status static_work(parac_task* task, parac_worker worker);
  static void static_terminate(volatile parac_task* task);
  static parac_status static_free_userdata(parac_task* task);
  static parac_status static_serialize(parac_task* task, parac_message* tgt);

  std::function<void()> m_terminationFunc;
  std::shared_ptr<cubesource::Source> m_cubeSource;
  std::mutex m_terminationMutex;

  friend class ::cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("source", m_cubeSource));
  }

  std::unique_ptr<NoncopyOStringstream> m_serializationOutStream;
  volatile QBFSolverTask** m_cleanupSelfPointer = nullptr;
};
}
