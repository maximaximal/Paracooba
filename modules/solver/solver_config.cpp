#include "solver_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <thread>

namespace parac::solver {
SolverConfig::SolverConfig(parac_config* config) {
  m_config = parac_config_reserve(config, static_cast<size_t>(Entry::_COUNT));

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::DeactivatePredefinedCubes)],
    "deactivate-predefined-cubes",
    "Deactivate trying all predefined cubes from the given "
    "iCNF file before going into resplitting options. ");
  m_config[static_cast<size_t>(Entry::DeactivatePredefinedCubes)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::DeactivatePredefinedCubes)].type =
    PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::DeactivatePredefinedCubes)]
    .default_value.boolean_switch = false;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::CaDiCaLCubes)],
    "cadical-cubes",
    "Use CaDiCaL to cube the formula");
  m_config[static_cast<size_t>(Entry::CaDiCaLCubes)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::CaDiCaLCubes)].type = PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::CaDiCaLCubes)]
    .default_value.boolean_switch = false;

  parac_config_entry_set_str(&m_config[static_cast<size_t>(Entry::Resplit)],
                             "resplit",
                             "Resplit cubes if they take too long");
  m_config[static_cast<size_t>(Entry::Resplit)].registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::Resplit)].type = PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::Resplit)].default_value.boolean_switch =
    false;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::InitialCubeDepth)],
    "initial-cube-depth",
    "Initial size of the cubes (requires option --cadical-cubes) to have an "
    "efect.");
  m_config[static_cast<size_t>(Entry::InitialCubeDepth)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::InitialCubeDepth)].type =
    PARAC_TYPE_UINT16;
  m_config[static_cast<size_t>(Entry::InitialCubeDepth)].default_value.uint16 =
    (1 + static_cast<int>(std::log2(10 * std::thread::hardware_concurrency())));

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)],
    "initial-minimal-cube-depth",
    "Minimal initial size of the cubes when lookahead cubing is too slow "
    "(requires option --cadical-cubes) to have an efect.");
  m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)].type =
    PARAC_TYPE_UINT16;
  m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)]
    .default_value.uint16 =
    (1 +
     static_cast<int>(std::log2(10 * std::thread::hardware_concurrency()))) /
    2;

  parac_config_entry_set_str(&m_config[static_cast<size_t>(Entry::MarchCubes)],
                             "march-cubes",
                             "Call March to split cubes.");
  m_config[static_cast<size_t>(Entry::MarchCubes)].registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::MarchCubes)].type = PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::MarchCubes)]
    .default_value.boolean_switch = false;
}

void
SolverConfig::extractFromConfigEntries() {
  m_predefinedCubes =
    !m_config[static_cast<size_t>(Entry::DeactivatePredefinedCubes)]
       .value.boolean_switch;
  m_CaDiCaLCubes =
    m_config[static_cast<size_t>(Entry::CaDiCaLCubes)].value.boolean_switch;
  m_resplit =
    m_config[static_cast<size_t>(Entry::Resplit)].value.boolean_switch;
  m_initialCubeDepth =
    m_config[static_cast<size_t>(Entry::InitialCubeDepth)].value.uint16;
  m_initialMinimalCubeDepth =
    m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)].value.uint16;
  m_marchCubes =
    m_config[static_cast<size_t>(Entry::MarchCubes)].value.boolean_switch;
}
}
