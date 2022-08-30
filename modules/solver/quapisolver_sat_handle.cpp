#include "quapisolver_sat_handle.hpp"
#include "cadical/cadical.hpp"
#include "cadical_handle.hpp"
#include "paracooba/common/status.h"

#include <paracooba/common/log.h>
#include <quapi/quapi.h>

namespace parac::solver {
class ClauseCounter : public CaDiCaL::ClauseIterator {
  public:
  virtual bool clause(const std::vector<int>& c) noexcept {
    (void)c;
    ++clausecount;
    return true;
  }
  int clausecount = 0;
};

int
countClausesInCadical(CaDiCaL::Solver& s) {
  ClauseCounter c;
  s.traverse_clauses(c);
  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Clause walker gathered {} clauses from CaDiCaL.",
            c.clausecount);
  return c.clausecount;
}

QuapiSolverHandle::QuapiSolverHandle(CaDiCaLHandle& cadicalHandle,
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
                       cadicalHandle.solver().vars(),
                       countClausesInCadical(cadicalHandle.solver()),
                       prefixdepth,
                       SAT_regex ? SAT_regex->c_str() : nullptr,
                       UNSAT_regex ? UNSAT_regex->c_str() : nullptr),
            &quapi_release)
  , m_name(name ? *name : "QuapiSolver:" + path)
  , m_prefixdepth(prefixdepth) {
  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Initiate QuapiSolverHandle {}.", m_name);

  if(!m_quapi) {
    parac_log(PARAC_SOLVER,
              PARAC_FATAL,
              "Could not initiate QuapiSolverHandle {}!",
              m_name);
    return;
  }

  {
    class ClauseWalker : public CaDiCaL::ClauseIterator {
      public:
      ClauseWalker(quapi_solver* s)
        : s(s) {}

      quapi_solver* s;
      virtual bool clause(const std::vector<int>& c) noexcept {
        for(int l : c) {
          quapi_add(s, l);
        }
        quapi_add(s, 0);
        return true;
      }
    };
    ClauseWalker w{ m_quapi.get() };
    cadicalHandle.solver().traverse_clauses(w);
  }

  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Fully initiated QuapiSolverHandle {}.", m_name);
}

QuapiSolverHandle::~QuapiSolverHandle() = default;

void
QuapiSolverHandle::assumeCube(const CubeIteratorRange& cube) {
  if(!m_quapi)
    return;

  if(static_cast<int>(cube.size()) > m_prefixdepth) {
    parac_log(
      PARAC_SOLVER,
      PARAC_LOCALERROR,
      "Cube {} is too long for Quapisolver {}! Internal depth is only {}.",
      cube,
      name(),
      m_prefixdepth);
    return;
  }

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
  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Terminating QuapiSolverHandle {}", name());
  quapi_terminate(m_quapi.get());
  parac_log(
    PARAC_SOLVER, PARAC_DEBUG, "Terminated QuapiSolverHandle {}", name());
}
}
