#include "cowsolver_handle.hpp"
#include "paracooba/common/status.h"
#include "parser_qbf.hpp"

#include <cowipasir/cowipasir.h>
#include <paracooba/common/log.h>

namespace parac::solver_qbf {
CowSolverHandle::CowSolverHandle(const Parser& parser,
                                 std::string path,
                                 const char** argv,
                                 const char** envp,
                                 int prefixdepth,
                                 std::optional<std::string> name,
                                 std::optional<std::string> SAT_regex,
                                 std::optional<std::string> UNSAT_regex)
  : m_cowipasir(cowipasir_init(path.c_str(),
                               argv,
                               envp,
                               parser.highestLiteral(),
                               parser.clauseCount(),
                               prefixdepth,
                               SAT_regex ? SAT_regex->c_str() : nullptr,
                               UNSAT_regex ? UNSAT_regex->c_str() : nullptr),
                &cowipasir_release)
  , m_name(name ? *name : "CowSolver:" + path) {
  parac_log(PARAC_SOLVER, PARAC_DEBUG, "Initiate CowSolverHandle {}.", m_name);

  if(!m_cowipasir) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not initiate CowSolverHandle {}!",
              m_name);
    return;
  }

  for(auto q : parser.quantifiers()) {
    cowipasir_quantify(m_cowipasir.get(), q.lit);
  }
  for(auto l : parser.literals()) {
    cowipasir_add(m_cowipasir.get(), l);
  }

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Fully initiated CowSolverHandle {}.", m_name);
}

CowSolverHandle::~CowSolverHandle() = default;

void
CowSolverHandle::assumeCube(const CubeIteratorRange& cube) {
  if(!m_cowipasir)
    return;

  for(auto l : cube) {
    // We don't need the 0 here.
    if(l == 0)
      continue;

    cowipasir_assume(m_cowipasir.get(), l);
  }
}

inline static parac_status
solve_inner(cowipasir_solver* s, volatile bool& terminated) {
  terminated = false;
  int result = cowipasir_solve(s);
  switch(result) {
    case 10:
      return PARAC_SAT;
    case 20:
      return PARAC_UNSAT;
    case 0:
      if(terminated)
        return PARAC_ABORTED;
      else
        return PARAC_UNKNOWN;
  }
  return PARAC_UNKNOWN;
}

parac_status
CowSolverHandle::solve() {
  if(!m_cowipasir)
    return PARAC_ABORTED;

  parac_log(PARAC_SOLVER, PARAC_TRACE, "Begin solving with {}.", m_name);
  parac_status s = solve_inner(m_cowipasir.get(), m_terminated);
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Finished solving with {}. Result {}.",
            m_name,
            s);
  return s;
}
void
CowSolverHandle::terminate() {
  m_terminated = true;
  cowipasir_terminate(m_cowipasir.get());
}

}
