#include "cadical_handle.hpp"
#include "cadical_terminator.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/path.h"
#include "paracooba/common/status.h"
#include "paracooba/module.h"
#include "solver_assignment.hpp"

#include "paracooba/common/timeout.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/solver/cube_iterator.hpp"

#include <cadical/cadical.hpp>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string_view>
#include <unistd.h>
#include <vector>

#ifdef __FreeBSD__
#define PARAC_DEFAULT_TEMP_PATH "/tmp"
#endif

#ifdef __linux__
#define PARAC_DEFAULT_TEMP_PATH "/dev/shm"
#endif

namespace parac::solver {
struct CaDiCaLHandle::Internal {
  Internal(parac_handle& handle,
           bool& stop,
           parac_id originatorId,
           std::shared_ptr<Cube> pregeneratedCubes = std::make_shared<Cube>(),
           std::shared_ptr<std::vector<size_t>> pregeneratedCubesJumplist =
             std::make_shared<std::vector<size_t>>())
    : handle(handle)
    , terminator(stop)
    , originatorId(originatorId)
    , pregeneratedCubes(pregeneratedCubes)
    , pregeneratedCubesJumplist(pregeneratedCubesJumplist) {}
  parac_handle& handle;
  CaDiCaL::Solver solver;
  size_t pregeneratedCubesCount = 0;
  size_t normalizedPathLength = 0;
  int vars;
  bool incremental;
  CaDiCaLTerminator terminator;
  std::string path;
  /// Only in parsed handle. The file should be deleted at the end (for string
  /// input).
  std::string pathToDelete;
  parac_id originatorId;
  std::shared_ptr<std::vector<Literal>> pregeneratedCubes;
  std::shared_ptr<std::vector<size_t>> pregeneratedCubesJumplist;
};

CaDiCaLHandle::CaDiCaLHandle(parac_handle& handle,
                             bool& stop,
                             parac_id originatorId)
  : m_internal(std::make_unique<Internal>(handle, stop, originatorId)) {}
CaDiCaLHandle::CaDiCaLHandle(CaDiCaLHandle& o)
  : m_internal(
      std::make_unique<Internal>(o.m_internal->handle,
                                 o.m_internal->terminator.stopRef(),
                                 o.m_internal->originatorId,
                                 o.m_internal->pregeneratedCubes,
                                 o.m_internal->pregeneratedCubesJumplist)) {
  // Copy from other solver to this one.
  o.solver().copy(solver());

  m_internal->pregeneratedCubesCount = o.m_internal->pregeneratedCubesCount;
  m_internal->normalizedPathLength = o.m_internal->normalizedPathLength;
  m_internal->vars = o.m_internal->vars;
  m_internal->incremental = o.m_internal->incremental;
  m_internal->path = o.m_internal->path;
  m_internal->originatorId = o.m_internal->originatorId;
}
CaDiCaLHandle::~CaDiCaLHandle() {
  if(m_internal->pathToDelete != "") {
    std::remove(m_internal->pathToDelete.c_str());
    parac_log(
      PARAC_SOLVER,
      PARAC_TRACE,
      "Removed temp file \"{}\" created in order to parse DIMACS from string.",
      m_internal->pathToDelete);
  }
}

CaDiCaL::Solver&
CaDiCaLHandle::solver() {
  return m_internal->solver;
}

std::pair<parac_status, std::string>
CaDiCaLHandle::prepareString(std::string_view iCNF) {
  auto h = std::hash<std::string_view>{}(iCNF);
  std::string path =
    PARAC_DEFAULT_TEMP_PATH "/paracooba-tmp-dimacs-file-" + std::to_string(h);
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Writing temp file \"{}\" in order to parse DIMACS from string.",
            path);
  {
    std::ofstream out(path);
    out << iCNF;
    out.flush();
  }
  m_internal->pathToDelete = path;
  if(m_internal->handle.input_file) {
    m_internal->handle.input_file = m_internal->pathToDelete.c_str();
  }
  return { PARAC_OK, path };
}

parac_status
CaDiCaLHandle::parseFile(const std::string& path) {
  if(path == "-") {
    m_internal->path = "/dev/stdin";
  } else {
    m_internal->path = path;
  }

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Start to parse DIMACS file \"{}\".",
            m_internal->path);

  const char* parseStatus =
    m_internal->solver.read_dimacs(m_internal->path.c_str(),
                                   m_internal->vars,
                                   1,
                                   m_internal->incremental,
                                   *m_internal->pregeneratedCubes);

  if(parseStatus != 0) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not parse DIMACS file \"{}\"! Error: {}",
              m_internal->path,
              parseStatus);
    return PARAC_PARSE_ERROR;
  }

  if(m_internal->pregeneratedCubes->size()) {
    generateJumplist();
  }

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Finished parsing DIMACS file \"{}\" with {} variables and {} "
            "pregenerated cubes. Normalized path length is {}.",
            m_internal->path,
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

  size_t beginPos = (*m_internal->pregeneratedCubesJumplist)[id];
  size_t endPos = (*m_internal->pregeneratedCubesJumplist)[id + 1];

  auto begin = &(*m_internal->pregeneratedCubes)[beginPos];
  auto end = &(*m_internal->pregeneratedCubes)[endPos] - 1;

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

size_t
CaDiCaLHandle::getPregeneratedCubesCount() const {
  return m_internal->pregeneratedCubesCount;
}
size_t
CaDiCaLHandle::getNormalizedPathLength() const {
  return m_internal->normalizedPathLength;
}

void
CaDiCaLHandle::generateJumplist() {
  parac_log(
    PARAC_SOLVER, PARAC_TRACE, "Begin parsing supplied cubes into jumplist.");

  m_internal->pregeneratedCubesJumplist->clear();

  size_t i = 0, lastStart = 0;
  for(int lit : *m_internal->pregeneratedCubes) {
    if(lit == 0) {
      m_internal->pregeneratedCubesJumplist->push_back(lastStart);
      lastStart = i + 1;
    }
    ++i;
  }

  m_internal->pregeneratedCubesCount =
    m_internal->pregeneratedCubesJumplist->size();

  m_internal->pregeneratedCubesJumplist->push_back(lastStart);

  float log = std::log2f(m_internal->pregeneratedCubesCount);
  m_internal->normalizedPathLength = std::ceil(log);

  parac_log(
    PARAC_SOLVER,
    PARAC_TRACE,
    "Finished parsing supplied cubes into jumplist. Normalized path length: {}",
    m_internal->normalizedPathLength);
}

template<typename T>
static void
applyCubeAsAssumptionToSolver(T cube, CaDiCaL::Solver& s) {
  for(auto lit : cube) {
    assert(lit != 0);
    s.assume(lit);
  }
}

void
CaDiCaLHandle::applyCubeAsAssumption(CubeIteratorRange cube) {
  applyCubeAsAssumptionToSolver(cube, m_internal->solver);
}

void
CaDiCaLHandle::applyCubeAsAssumption(Cube cube) {
  applyCubeAsAssumptionToSolver(cube, m_internal->solver);
}

parac_status
CaDiCaLHandle::solve() {
  int r = m_internal->solver.solve();
  switch(r) {
    case 0:
      return PARAC_ABORTED;
    case 10:
      parac_log(
        PARAC_SOLVER,
        PARAC_TRACE,
        "Satisfying assignment found! Encoding before returning result.");
      m_solverAssignment = std::make_unique<SolverAssignment>();
      m_solverAssignment->SerializeAssignmentFromSolver(m_internal->vars,
                                                        m_internal->solver);
      parac_log(PARAC_SOLVER,
                PARAC_TRACE,
                "Finished encoding satisfying result! Encoded {} variables.",
                m_solverAssignment->varCount());
      return PARAC_SAT;
    case 20:
      return PARAC_UNSAT;
    default:
      return PARAC_UNKNOWN;
  }
}

void
CaDiCaLHandle::terminate() {
  m_internal->terminator.terminateLocally();
  m_internal->solver.terminate();
}

std::unique_ptr<SolverAssignment>
CaDiCaLHandle::takeSolverAssignment() {
  return std::move(m_solverAssignment);
}

std::pair<parac_status, std::optional<std::pair<Cube, Cube>>>
CaDiCaLHandle::resplitOnce(parac_path path, Cube literals) {
  parac_log(
    PARAC_CUBER, PARAC_TRACE, "CNF formula for path {} splitted.", path);

  Cube literals2(literals);
  for(auto lit : literals) {
    parac_log(PARAC_CUBER, PARAC_TRACE, "Cube lit: {}", lit);
    m_internal->solver.assume(lit);
  }

  Literal lit_to_split = m_internal->solver.lookahead();
  m_internal->solver.reset_assumptions();
  if(lit_to_split == 0) {
    if(m_internal->solver.state() == CaDiCaL::SATISFIED) {
      return { PARAC_SAT, {} };
    } else if(m_internal->solver.state() == CaDiCaL::UNSATISFIED) {
      return { PARAC_UNSAT, {} };
    } else {
      parac_log(PARAC_CUBER,
                PARAC_TRACE,
                "Cannot split further, as lookahead returned 0");
      return { PARAC_NO_SPLITS_LEFT, {} };
    }
  }

  parac_log(PARAC_CUBER,
            PARAC_TRACE,
            "CNF formula for path {} resplitted on literal {}.",
            path,
            lit_to_split);
  literals.push_back(lit_to_split);
  literals2.push_back(-lit_to_split);
  m_internal->solver.reset_assumptions();
  return { PARAC_SPLITTED, { { literals, literals2 } } };
}

static parac_timeout*
setTimeout(parac_handle& paracHandle,
           uint64_t ms,
           void* userdata,
           parac_timeout_expired expired_cb) {
  parac_module* paracCommMod = paracHandle.modules[PARAC_MOD_COMMUNICATOR];
  if(!paracCommMod)
    return nullptr;
  parac_module_communicator* paracComm = paracCommMod->communicator;
  if(!paracComm)
    return nullptr;

  return paracComm->set_timeout(paracCommMod, ms, userdata, expired_cb);
}

parac_status
CaDiCaLHandle::lookahead(size_t depth, size_t min_depth) {
  parac_log(PARAC_CUBER, PARAC_TRACE, "Generating cubes of length {}", depth);

  auto& pregeneratedCubes = *m_internal->pregeneratedCubes;

  if(!pregeneratedCubes.empty()) {
    parac_log(PARAC_CUBER,
              PARAC_LOCALWARNING,
              "Pregenerated cubes already found in formula! This will "
              "overwrite them, because --cadical-cubes was used.",
              depth);
    pregeneratedCubes.clear();
    m_internal->pregeneratedCubesCount = 0;
    m_internal->pregeneratedCubesJumplist->clear();
  }
  assert(pregeneratedCubes.empty());

  m_lookaheadTimeout =
    setTimeout(m_internal->handle, 30000, this, [](parac_timeout* t) {
      CaDiCaLHandle* handle = static_cast<CaDiCaLHandle*>(t->expired_userdata);
      handle->m_lookaheadTimeout = nullptr;
      handle->m_interruptedLookahead = true;
      handle->terminate();
    });

  auto cubes{ m_internal->solver.generate_cubes(depth, min_depth) };

  if(m_lookaheadTimeout) {
    m_lookaheadTimeout->cancel(m_lookaheadTimeout);
  }

  m_interruptedLookahead = true;
  if(cubes.status == 20)
    return PARAC_UNSAT;
  else if(cubes.status == 10)
    return PARAC_SAT;
  std::vector<int> flatCubes;
  size_t max_depth = 0;

  for(const auto& cube : cubes.cubes) {
    std::copy(begin(cube), end(cube), std::back_inserter(pregeneratedCubes));
    pregeneratedCubes.emplace_back(0);
    max_depth = std::max(max_depth, cube.size());
  }

  parac_log(PARAC_CUBER,
            PARAC_TRACE,
            "Generated {} cubes. Max depth = {}",
            cubes.cubes.size(),
            max_depth);

  generateJumplist();

  parac_log(PARAC_SOLVER,
            PARAC_DEBUG,
            "Finished generating CaDiCaL cubes for DIMACS file \"{}\" with {} "
            "variables and {} "
            "pregenerated cubes. Normalized path length is {}.",
            m_internal->path,
            m_internal->vars,
            m_internal->pregeneratedCubesCount,
            m_internal->normalizedPathLength);

  return PARAC_SPLITTED;
}
}
