extern "C" {
#include <qdpll.h>
}

#include "depqbf_handle.hpp"

namespace parac::solver_qbf {
DepQBFHandle::DepQBFHandle(const Parser& parser)
  : m_qdpll(qdpll_create(), &qdpll_delete) {}
DepQBFHandle::~DepQBFHandle() {}

void
DepQBFHandle::assumeCube(const Cube& cube) {
  for(Literal lit : cube) {
    qdpll_assume(m_qdpll.get(), lit);
  }
}

parac_status
DepQBFHandle::solve() {}
}
