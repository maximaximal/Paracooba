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

  enum class Entry { _COUNT };

  private:
  parac_config_entry* m_config = nullptr;

  parac_id m_originatorId = 0;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("originatorId", m_originatorId));
  }
};
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config);
}
