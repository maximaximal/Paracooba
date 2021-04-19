#include "solver_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <thread>

namespace parac::solver {
SolverConfig::SolverConfig(parac_config* config, parac_id localId) {
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
    &m_config[static_cast<size_t>(Entry::DisableLocalKissat)],
    "disable-local-kissat",
    "Disable starting Kissat parallel to the distributed cube-and-conquer "
    "algorithm. Automatically disabled if available worker count < 2.");
  m_config[static_cast<size_t>(Entry::DisableLocalKissat)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::DisableLocalKissat)].type =
    PARAC_TYPE_SWITCH;
  m_config[static_cast<size_t>(Entry::DisableLocalKissat)]
    .default_value.boolean_switch = false;

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

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::InitialSplitTimeoutMS)],
    "initial-split-timeout-ms",
    "Initial CaDiCaL cubes split timeout at splitting phase (using "
    "lookahead).");
  m_config[static_cast<size_t>(Entry::InitialSplitTimeoutMS)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::InitialSplitTimeoutMS)].type =
    PARAC_TYPE_UINT32;
  m_config[static_cast<size_t>(Entry::InitialSplitTimeoutMS)]
    .default_value.uint16 = 30000;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::ConcurrentCubeTreeCount)],
    "concurrent-cube-tree-count",
    "Number of cube-trees to concurrently build at the beginning. Uses "
    "different initial splitting literals to create concurrent root entries "
    "each trying to solve the same problem with different splits.");
  m_config[static_cast<size_t>(Entry::ConcurrentCubeTreeCount)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::ConcurrentCubeTreeCount)].type =
    PARAC_TYPE_UINT16;
  m_config[static_cast<size_t>(Entry::ConcurrentCubeTreeCount)]
    .default_value.uint16 = 1;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(
      Entry::DistributeCubeTreeLearntClausesMaxLevel)],
    "distribute-tree-learnt-clauses-max-level",
    "When using --concurrent-cube-tree-count the sibling cube trees may finish "
    "branches early. This variable controls up until which depth (levels in "
    "the cube tree) the UNSAT cube should be distributed as learnt clause to "
    "all other instances. 0 disables distributing learnt cube clauses.");
  m_config[static_cast<size_t>(Entry::DistributeCubeTreeLearntClausesMaxLevel)]
    .registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::DistributeCubeTreeLearntClausesMaxLevel)]
    .type = PARAC_TYPE_UINT16;
  m_config[static_cast<size_t>(Entry::DistributeCubeTreeLearntClausesMaxLevel)]
    .default_value.uint16 = 0;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::FastSplitMultiplicationFactor)],
    "cubing-fast-split-multiplication-factor",
    "The multiplication factor when in fast-split mode (when not enough work "
    "is in the queue)");
  m_config[static_cast<size_t>(Entry::FastSplitMultiplicationFactor)]
    .registrar = PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::FastSplitMultiplicationFactor)].type =
    PARAC_TYPE_FLOAT;
  m_config[static_cast<size_t>(Entry::FastSplitMultiplicationFactor)]
    .default_value.f = m_fastSplitMultiplicationFactor;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::SplitMultiplicationFactor)],
    "cubing-split-multiplication-factor",
    "The multiplication factor when in normal splitting mode");
  m_config[static_cast<size_t>(Entry::SplitMultiplicationFactor)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::SplitMultiplicationFactor)].type =
    PARAC_TYPE_FLOAT;
  m_config[static_cast<size_t>(Entry::SplitMultiplicationFactor)]
    .default_value.f = m_splitMultiplicationFactor;

  m_originatorId = localId;
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
  m_disableLocalKissat =
    m_config[static_cast<size_t>(Entry::DisableLocalKissat)]
      .value.boolean_switch;
  m_initialCubeDepth =
    m_config[static_cast<size_t>(Entry::InitialCubeDepth)].value.uint16;
  m_initialMinimalCubeDepth =
    m_config[static_cast<size_t>(Entry::InitialMinimalCubeDepth)].value.uint16;
  m_initialSplitTimeoutMS =
    m_config[static_cast<size_t>(Entry::InitialSplitTimeoutMS)].value.uint32;
  m_fastSplitMultiplicationFactor =
    m_config[static_cast<size_t>(Entry::FastSplitMultiplicationFactor)].value.f;
  m_splitMultiplicationFactor =
    m_config[static_cast<size_t>(Entry::SplitMultiplicationFactor)].value.f;
  m_concurrentCubeTreeCount =
    m_config[static_cast<size_t>(Entry::ConcurrentCubeTreeCount)].value.uint16;
  m_distributeCubeTreeLearntClausesMaxLevel =
    m_config[static_cast<size_t>(
               Entry::DistributeCubeTreeLearntClausesMaxLevel)]
      .value.uint16;
}
std::ostream&
operator<<(std::ostream& o, const SolverConfig& config) {
  return o << "PredefinedCubes:" << config.PredefinedCubes()
           << ", CaDiCaLCubes:" << config.CaDiCaLCubes()
           << ", Resplit:" << config.Resplit()
           << ", DisableLocalKissat:" << config.DisableLocalKissat()
           << ", InitialCubeDepth:" << config.InitialCubeDepth()
           << ", ConcurrentCubeTreeCount:" << config.ConcurrentCubeTreeCount()
           << ", DistributeCubeTreeLearntClausesMaxLevel:"
           << config.DistributeCubeTreeLearntClausesMaxLevel()
           << ", FastSplitMultiplicationFactor:"
           << config.FastSplitMultiplicationFactor()
           << ", SplitMultiplicationFactor:"
           << config.SplitMultiplicationFactor();
}
}
