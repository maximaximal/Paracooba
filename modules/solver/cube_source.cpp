#include "cube_source.hpp"
#include "cadical_handle.hpp"
#include "cadical_manager.hpp"

#include <paracooba/common/path.h>

#include <cassert>

namespace parac::solver::cubesource {
CubeIteratorRange
PathDefined::cube(parac_path p, CaDiCaLManager& mgr) {
  CubeIteratorRange r = mgr.getCubeFromPath(p);
  //assert(!r.contains0());
  return r;
}

bool
PathDefined::split(parac_path p,
                   CaDiCaLManager& mgr,
                   CaDiCaLHandle& handle,
                   bool& left,
                   bool& right) const {
  (void)mgr;
  left = handle.pathIsInNormalizedRange(parac_path_get_next_left(p));
  right = handle.pathIsInNormalizedRange(parac_path_get_next_right(p));
  return left || right;
}

CubeIteratorRange
Supplied::cube(parac_path p, CaDiCaLManager& mgr) {
  (void)p;
  (void)mgr;
  return m_cube;
}
std::unique_ptr<Source>
PathDefined::copy() const {
  return std::make_unique<PathDefined>(*this);
}

bool
Supplied::split(parac_path p,
                CaDiCaLManager& mgr,
                CaDiCaLHandle& handle,
                bool& left,
                bool& right) const {
  (void)p;
  (void)mgr;
  (void)handle;
  left = false;
  right = false;
  return false;
}
std::unique_ptr<Source>
Supplied::copy() const {
  return std::make_unique<Supplied>(*this);
}

CubeIteratorRange
Unspecified::cube(parac_path p, CaDiCaLManager& mgr) {
  (void)p;
  (void)mgr;
  return CubeIteratorRange();
}

bool
Unspecified::split(parac_path p,
                   CaDiCaLManager& mgr,
                   CaDiCaLHandle& handle,
                   bool& left,
                   bool& right) const {
  (void) p;
  (void)mgr;
  (void)handle;
  left = false;
  right = false;
  return false;
}
std::unique_ptr<Source>
Unspecified::copy() const {
  return std::make_unique<Unspecified>(*this);
}
}
