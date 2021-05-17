#include "solver_qbf_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <thread>

namespace parac::solver_qbf {
SolverConfig::SolverConfig(parac_config* config, parac_id localId) {
  // m_config = parac_config_reserve(config,
  // static_cast<size_t>(Entry::_COUNT));
  m_originatorId = localId;
}

void
SolverConfig::extractFromConfigEntries() {}
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config) {
  return o;
}
}
