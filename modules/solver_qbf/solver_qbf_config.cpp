#include "solver_qbf_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <thread>

namespace parac::solver_qbf {
SolverConfig::SolverConfig(parac_config* config, parac_id localId) {
  m_config = parac_config_reserve(config, static_cast<size_t>(Entry::_COUNT));
  m_originatorId = localId;

  parac_config_entry_set_str(&m_config[static_cast<size_t>(Entry::UseDepQBF)],
                             "use-depqbf",
                             "Select DepQBF as the QBF solver to be used");
  m_config[static_cast<size_t>(Entry::UseDepQBF)].registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::UseDepQBF)].type = PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::UseDepQBF)].default_value.boolean_switch =
    false;
}

void
SolverConfig::extractFromConfigEntries() {
  m_useDepQBF =
    m_config[static_cast<size_t>(Entry::UseDepQBF)].value.boolean_switch;
}
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config) {
  return o;
}
}
