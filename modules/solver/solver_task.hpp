#pragma once

#include "paracooba/common/path.h"
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>
#include <paracooba/solver/types.hpp>

#include "cube_source.hpp"

struct parac_task;
struct parac_message;

namespace parac::solver {
class CaDiCaLManager;

class SolverTask {
  public:
  SolverTask();
  ~SolverTask();

  void init(CaDiCaLManager& manager,
            parac_task& task,
            std::shared_ptr<cubesource::Source> cubesource);

  parac_status work(parac_worker worker);
  parac_status serialize_to_msg(parac_message* tgt_msg);

  static SolverTask& createRoot(parac_task& task, CaDiCaLManager& manager);
  static SolverTask& create(parac_task& task,
                            CaDiCaLManager& manager,
                            std::shared_ptr<cubesource::Source> source);
  static parac_status static_work(parac_task* task, parac_worker worker);
  static parac_status static_serialize(parac_task* task,
                                       parac_message* tgt_msg);

  parac_path path() const;

  private:
  CaDiCaLManager* m_manager = nullptr;
  parac_task* m_task = nullptr;

  std::shared_ptr<cubesource::Source> m_cubeSource;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("path", path()),
       cereal::make_nvp("source", *m_cubeSource));
  }
};
}
