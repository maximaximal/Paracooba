#include "solver_qbf_config.hpp"
#include "paracooba/common/config.h"
#include "paracooba/common/log.h"
#include "paracooba/common/types.h"

#include <cmath>
#include <stdexcept>
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
  m_config[static_cast<size_t>(Entry::TreeDepth)].default_value.uint64 =
    (1 +
     static_cast<int>(std::log2(10 * std::thread::hardware_concurrency()))) /
    2;

  parac_config_entry_set_str(
    &m_config[static_cast<size_t>(Entry::IntegerBasedSplits)],
    "int-split",
    "Define once for every layer that should be split maximum X times instead "
    "of direct literal expansion.");
  m_config[static_cast<size_t>(Entry::IntegerBasedSplits)].registrar =
    PARAC_MOD_SOLVER;
  m_config[static_cast<size_t>(Entry::IntegerBasedSplits)].type =
    PARAC_TYPE_VECTOR_STR;
  m_config[static_cast<size_t>(Entry::IntegerBasedSplits)]
    .default_value.string_vector.strings = nullptr;
  m_config[static_cast<size_t>(Entry::IntegerBasedSplits)]
    .default_value.string_vector.size = 0;
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

  auto& intSplitsE = m_config[static_cast<size_t>(Entry::IntegerBasedSplits)];
  auto& intSplitsArr = intSplitsE.value.string_vector.strings;
  auto& intSplitsCount = intSplitsE.value.string_vector.size;
  if(intSplitsCount > 0) {
    m_integerBasedSplits.resize(intSplitsCount);
    for(size_t i = 0; i < intSplitsCount; ++i) {
      const char* s = intSplitsArr[i];
      try {
        int s_ = std::stoi(s);
        if(s_ < 0) {
          throw(std::invalid_argument("Integer splits may only be 0 or >0!"));
        }
        m_integerBasedSplits[i] = s_;
      } catch(std::invalid_argument& e) {
        parac_log(PARAC_SOLVER,
                  PARAC_LOCALERROR,
                  "Cannot parse argument {} for --int-split because of "
                  "invalid-argument exception! Error: {}. Setting level to 0.",
                  s,
                  e.what(),
                  i);
      } catch(std::out_of_range& e) {
        parac_log(PARAC_SOLVER,
                  PARAC_LOCALERROR,
                  "Cannot parse all arguments for --int-split because of "
                  "out-of-range exception! Error: {}. Setting level {} to 0.",
                  s,
                  e.what(),
                  i);
      }
    }

    if(m_integerBasedSplits.size() > 0) {
      parac_log(PARAC_SOLVER,
                PARAC_DEBUG,
                "Specified int-splits: {}",
                fmt::join(m_integerBasedSplits, " "));
    }
  }
}

// https://stackoverflow.com/a/15327567
static inline int
ceil_log2(unsigned long long x) {
  static const unsigned long long t[6] = {
    0xFFFFFFFF00000000ull, 0x00000000FFFF0000ull, 0x000000000000FF00ull,
    0x00000000000000F0ull, 0x000000000000000Cull, 0x0000000000000002ull
  };

  int y = (((x & (x - 1)) == 0) ? 0 : 1);
  int j = 32;
  int i;

  for(i = 0; i < 6; i++) {
    int k = (((x & t[i]) == 0) ? 0 : j);
    y += k;
    x >>= k;
    j >>= 1;
  }

  return y;
}

size_t
SolverConfig::integerBasedSplitsCurrentLength(size_t index) const {
  assert(index < m_integerBasedSplits.size());
  int intSplit = m_integerBasedSplits[index];
  if(intSplit != 0) {
    return ceil_log2(intSplit);
  }
  return 1;
}

size_t
SolverConfig::integerBasedSplitsCurrentIndex(size_t currentCubeLength) const {
  size_t i;
  for(i = 0; i < m_integerBasedSplits.size(); ++i) {
    size_t bits = integerBasedSplitsCurrentLength(i);
    if(currentCubeLength < bits)
      return i;
    currentCubeLength -= bits;
  }
  return i;
}

std::ostream&
operator<<(std::ostream& o, const SolverConfig& config) {
  o << "useDepQBF: " << config.useDepQBF()
    << ", tree-depth: " << config.treeDepth();
  o << ", int-splits:";
  if(config.integerBasedSplitsEnabled()) {
    for(int s : config.integerBasedSplits()) {
      o << " " << s;
    }
  } else {
    o << " disabled";
  }
  return o;
}
}
