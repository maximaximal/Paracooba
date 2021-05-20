#pragma once

#include <cstdint>

#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/common/types.h>

struct parac_config_entry;
struct parac_config;

namespace parac::solver_qbf {
class SolverConfig {
  public:
  SolverConfig() = default;
  explicit SolverConfig(parac_config* config, parac_id localId);
  ~SolverConfig() = default;

  void extractFromConfigEntries();

  parac_id originatorId() const { return m_originatorId; }
  bool useDepQBF() const { return m_useDepQBF; }
  int64_t treeDepth() const { return m_treeDepth; }

  enum class Entry { UseDepQBF, TreeDepth, _COUNT };

  private:
  parac_config_entry* m_config = nullptr;

  parac_id m_originatorId = 0;

  bool m_useDepQBF = false;
  uint64_t m_treeDepth;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("originatorId", m_originatorId),
       cereal::make_nvp("useDepQBF", m_useDepQBF),
       cereal::make_nvp("treeDepth", m_treeDepth));
  }
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
}
