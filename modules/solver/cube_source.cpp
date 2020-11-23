#include "cube_source.hpp"
#include "cadical_manager.hpp"

#include <paracooba/common/path.h>

namespace parac::solver::cubesource {
CubeIteratorRange
PathDefined::cube(parac_path p, CaDiCaLManager& mgr) {
  return mgr.getCubeFromPath(p);
}

CubeIteratorRange
Supplied::cube(parac_path p, CaDiCaLManager& mgr) {
  (void)p;
  (void)mgr;
  return m_cube;
}
}
