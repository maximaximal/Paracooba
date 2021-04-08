#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/common/path.h>
#include <paracooba/common/status.h>
#include <paracooba/common/types.h>
#include <paracooba/solver/types.hpp>

#include "cube_source.hpp"

struct parac_task;
struct parac_message;
struct parac_handle;
struct parac_timeout;

namespace parac {
class NoncopyOStringstream;
}

namespace parac::solver {
class CaDiCaLManager;
class SolverConfig;

class SolverTask {
  public:
  SolverTask();
  ~SolverTask();

  void init(CaDiCaLManager& manager,
            parac_task& task,
            std::shared_ptr<cubesource::Source> cubesource);

  parac_status work(parac_worker worker);
  parac_status serialize_to_msg(parac_message* tgt_msg);

  static SolverTask& create(parac_task& task,
                            CaDiCaLManager& manager,
                            std::shared_ptr<cubesource::Source> source);
  static parac_status static_work(parac_task* task, parac_worker worker);
  static parac_status static_serialize(parac_task* task,
                                       parac_message* tgt_msg);
  static void static_terminate(parac_task* task);

  parac_path& path();
  const parac_path& path() const;
  CaDiCaLManager* manager() { return m_manager; }

  struct CubeTreeElem {
    parac_task* task;
    Cube cube;
  };

  private:
  class FastSplit {
    public:
    operator bool() const { return fastSplit; }
    void half_tick(bool b) { local_situation = b; }
    void tick(bool b) {
      assert(beta <= alpha);
      const bool full_tick = (b || local_situation);
      if(full_tick)
        ++beta;
      else
        fastSplit = false;
      ++alpha;

      if(alpha == period) {
        fastSplit = (beta >= period / 2);
        beta = 0;
        alpha = 0;
        if(fastSplit)
          ++depth;
        else
          --depth;
      }
    }
    int split_depth() { return fastSplit ? depth : 0; }

    private:
    unsigned alpha = 0;
    unsigned beta = 0;
    bool fastSplit = true;
    const unsigned period = 8;
    int depth = 1;
    bool local_situation = true;
  };

  CaDiCaLManager* m_manager = nullptr;
  parac_task* m_task = nullptr;
  FastSplit m_fastSplit;
  parac_timeout* m_timeout = nullptr;
  std::atomic<CaDiCaLHandle*> m_activeHandle = nullptr;
  bool m_interruptSolving = false;

#ifndef NDEBUG
  std::atomic_bool m_working = false;
#endif

  std::shared_ptr<cubesource::Source> m_cubeSource;

  std::pair<parac_status, std::vector<CubeTreeElem>>
  resplitDepth(parac_path path, Cube literals, int depth);

  std::tuple<parac_status, std::vector<CubeTreeElem>, uint64_t> resplit(
    uint64_t durationMS);

  std::pair<parac_status, uint64_t> solveOrConditionallyAbort(
    const SolverConfig& config,
    CaDiCaLHandle& handle,
    uint64_t duration);

  friend class ::cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("source", m_cubeSource));
  }

  std::unique_ptr<NoncopyOStringstream> m_serializationOutStream;
};
}
