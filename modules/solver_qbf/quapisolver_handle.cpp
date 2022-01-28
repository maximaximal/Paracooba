#include "quapisolver_handle.hpp"
#include "paracooba/common/status.h"
#include "parser_qbf.hpp"

#include <paracooba/common/log.h>
#include <quapi/quapi.h>

namespace parac::solver_qbf {
QuapiSolverHandle::QuapiSolverHandle(const Parser& parser,
                                     std::string path,
                                     const char** argv,
                                     const char** envp,
                                     int prefixdepth,
                                     std::optional<std::string> name,
                                     std::optional<std::string> SAT_regex,
                                     std::optional<std::string> UNSAT_regex)
  : m_quapi(quapi_init(path.c_str(),
                       argv,
                       envp,
                       parser.highestLiteral(),
                       parser.clauseCount(),
                       std::min(parser.quantifiers().size(),
                                static_cast<size_t>(prefixdepth)),
                       SAT_regex ? SAT_regex->c_str() : nullptr,
                       UNSAT_regex ? UNSAT_regex->c_str() : nullptr),
            &quapi_release)
  , m_name(name ? *name : "QuapiSolver:" + path) {
  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Initiate QuapiSolverHandle {}.", m_name);

  if(!m_quapi) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not initiate QuapiSolverHandle {}!",
              m_name);
    return;
  }

  for(auto q : parser.quantifiers()) {
    quapi_quantify(m_quapi.get(), q.lit);
  }
  for(auto l : parser.literals()) {
    quapi_add(m_quapi.get(), l);
  }

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Fully initiated QuapiSolverHandle {}.", m_name);
}

QuapiSolverHandle::~QuapiSolverHandle() = default;

void
QuapiSolverHandle::assumeCube(const CubeIteratorRange& cube) {
  if(!m_quapi)
    return;

  for(auto l : cube) {
    // We don't need the 0 here.
    if(l == 0)
      continue;

    quapi_assume(m_quapi.get(), l);
  }
}

inline static parac_status
solve_inner(quapi_solver* s, volatile bool& terminated) {
  terminated = false;
  int result = quapi_solve(s);
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
QuapiSolverHandle::solve() {
  if(!m_quapi)
    return PARAC_ABORTED;

  parac_log(PARAC_SOLVER, PARAC_TRACE, "Begin solving with {}.", m_name);
  parac_status s = solve_inner(m_quapi.get(), m_terminated);
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Finished solving with {}. Result {}.",
            m_name,
            s);
  return s;
}
void
QuapiSolverHandle::terminate() {
  m_terminated = true;
  quapi_terminate(m_quapi.get());
}
}
