#include "solver_qbf_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/log.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <thread>
#include <type_traits>

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

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::TreeDepth)],
    "tree-depth",
    "Use the Cube-Tree up until the specified depth. 0 Deactivates the limit.");
  m_config[static_cast<size_t>(Entry::TreeDepth)].registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::TreeDepth)].type = PARAC_TYPE_UINT64;
  m_config[static_cast<size_t>(Entry::TreeDepth)].default_value.uint64 = 0;
}

void
SolverConfig::extractFromConfigEntries() {
  m_useDepQBF =
    m_config[static_cast<size_t>(Entry::UseDepQBF)].value.boolean_switch;
  m_treeDepth = m_config[static_cast<size_t>(Entry::TreeDepth)].value.uint64;

  // Choose one solver so that solving works correctly.
  if(!m_useDepQBF) {
    parac_log(
      PARAC_SOLVER,
      PARAC_TRACE,
      "Did not specify any QBF solver to use! Using DepQBF by default.");
    m_useDepQBF = true;
  }
}
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config) {
  o << "useDepQBF: " << config.useDepQBF();
  return o;
}
}
