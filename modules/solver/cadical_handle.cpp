#include "cadical_handle.hpp"
#include "cadical_terminator.hpp"
#include "fmt/core.h"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"

#include "paracooba/solver/cube_iterator.hpp"

#include <cadical/cadical.hpp>
#include <cassert>
#include <unistd.h>
#include <vector>

namespace parac::solver {
struct CaDiCaLHandle::Internal {
  Internal(bool& stop, parac_id originatorId)
    : terminator(stop)
    , originatorId(originatorId) {}
  CaDiCaL::Solver solver;
  std::vector<Literal> pregeneratedCubes;
  std::vector<size_t> pregeneratedCubesJumplist;
  size_t pregeneratedCubesCount = 0;
  size_t normalizedPathLength = 0;
  int vars;
  bool incremental;
  CaDiCaLTerminator terminator;
  std::string path;
  parac_id originatorId;
};

CaDiCaLHandle::CaDiCaLHandle(bool& stop, parac_id originatorId)
  : m_internal(std::make_unique<Internal>(stop, originatorId)) {}
CaDiCaLHandle::CaDiCaLHandle(CaDiCaLHandle& o)
  : m_internal(std::make_unique<Internal>(o.m_internal->terminator.stopRef(),
                                          o.m_internal->originatorId)) {
  // Copy from other solver to this one.
  o.solver().copy(solver());
}
CaDiCaLHandle::~CaDiCaLHandle() {}

CaDiCaL::Solver&
CaDiCaLHandle::solver() {
  return m_internal->solver;
}

parac_status
CaDiCaLHandle::parseFile(const std::string& path) {
  m_internal->path = path;

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Start to parse DIMACS file \"{}\".", path);

  const char* parseStatus =
    m_internal->solver.read_dimacs(path.c_str(),
                                   m_internal->vars,
                                   1,
                                   m_internal->incremental,
                                   m_internal->pregeneratedCubes);

  if(parseStatus != 0) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not parse DIMACS file \"{}\"! Error: {}",
              path,
              parseStatus);
    return PARAC_PARSE_ERROR;
  }

  if(m_internal->pregeneratedCubes.size()) {
    generateJumplist();
  }

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Finished parsing DIMACS file \"{}\" with {} variables and {} "
            "pregenerated cubes. Normalized path length is {}.",
            path,
            m_internal->vars,
            m_internal->pregeneratedCubesCount,
            m_internal->normalizedPathLength);

  m_hasFormula = true;

  return PARAC_OK;
}
const std::string&
CaDiCaLHandle::path() const {
  return m_internal->path;
}
parac_id
CaDiCaLHandle::originatorId() const {
  return m_internal->originatorId;
}

CubeIteratorRange
CaDiCaLHandle::getCubeFromId(CubeId id) const {
  if(id >= m_internal->pregeneratedCubesCount) {
    return CubeIteratorRange();
  }

  size_t beginPos = m_internal->pregeneratedCubesJumplist[id];
  size_t endPos = m_internal->pregeneratedCubesJumplist[id + 1];

  auto begin = &m_internal->pregeneratedCubes[beginPos];
  auto end = &m_internal->pregeneratedCubes[endPos];

  assert(begin);
  assert(end);

  return CubeIteratorRange(begin, end);
}
CubeIteratorRange
CaDiCaLHandle::getCubeFromPath(parac_path path) const {
  if(path.length != m_internal->normalizedPathLength) {
    // The path needs to be at the end of the cube tree, or no predefined
    // cubes can be received, as the cube would be ambiguous!
    return CubeIteratorRange();
  }

  parac_path_type depth_shifted = parac_path_get_depth_shifted(path);
  return getCubeFromId(depth_shifted);
}

void
CaDiCaLHandle::generateJumplist() {
  parac_log(
    PARAC_SOLVER, PARAC_TRACE, "Begin parsing supplied cubes into jumplist.");

  m_internal->pregeneratedCubesJumplist.clear();

  size_t i = 0, lastStart = 0;
  for(int lit : m_internal->pregeneratedCubes) {
    if(lit == 0) {
      m_internal->pregeneratedCubesJumplist.push_back(lastStart);
      lastStart = i + 1;
    }
    ++i;
  }

  m_internal->pregeneratedCubesCount =
    m_internal->pregeneratedCubesJumplist.size();

  m_internal->pregeneratedCubesJumplist.push_back(lastStart);

  float log = std::log2f(m_internal->pregeneratedCubesCount);
  m_internal->normalizedPathLength = std::ceil(log);

  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Finished parsing supplied cubes into jumplist.");
}

void
CaDiCaLHandle::applyCubeAsAssumption(CubeIteratorRange cube) {
  for(auto lit : cube) {
    m_internal->solver.assume(lit);
  }
}
parac_status
CaDiCaLHandle::solve() {
  int r = m_internal->solver.solve();
  switch(r) {
    case 0:
      return PARAC_ABORTED;
    case 10:
      return PARAC_SAT;
    case 20:
      return PARAC_UNSAT;
    default:
      return PARAC_UNKNOWN;
  }
}
}
