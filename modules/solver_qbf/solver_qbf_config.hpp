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

  bool integerBasedSplitsEnabled() const {
    return m_integerBasedSplits.size() > 0;
  }
  const std::vector<int>& integerBasedSplits() const {
    return m_integerBasedSplits;
  }
  size_t integerBasedSplitsCurrentIndex(size_t currentCubeLength) const;
  size_t integerBasedSplitsCurrentLength(size_t index) const;

  enum class Entry { UseDepQBF, TreeDepth, IntegerBasedSplits, _COUNT };

  private:
  parac_config_entry* m_config = nullptr;

  parac_id m_originatorId = 0;

  bool m_useDepQBF = false;
  uint64_t m_treeDepth;
  std::vector<int> m_integerBasedSplits;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("originatorId", m_originatorId),
       cereal::make_nvp("useDepQBF", m_useDepQBF),
       cereal::make_nvp("treeDepth", m_treeDepth),
       cereal::make_nvp("integerBasedSplits", m_integerBasedSplits));
  }
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
}
