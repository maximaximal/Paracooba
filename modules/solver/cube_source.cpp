#include "cube_source.hpp"
#include "cadical_handle.hpp"
#include "cadical_manager.hpp"

#include <paracooba/common/log.h>
#include <paracooba/common/path.h>

#include <cassert>

namespace parac::solver::cubesource {
CubeIteratorRange
PathDefined::cube(parac_path p, CaDiCaLManager& mgr) {
  CubeIteratorRange r = mgr.getCubeFromPath(p);
  // Useful assert during debugging:
  // assert(!r.contains0());
  return r;
}

PathDefined::PathDefined() {}
PathDefined::~PathDefined() {}

bool
PathDefined::split(parac_path p,
                   CaDiCaLManager& mgr,
                   const CaDiCaLHandle& handle,
                   bool& left,
                   bool& right) const {
  (void)mgr;

  if(handle.getNormalizedPathLength() < static_cast<size_t>(p.length) + 1) {
    left = false;
    right = false;
    return false;
  }

  // Left is always ok, as only right splits increase the counter. Right splits
  // have to be done carefully, as one right split equates to one more possible
  // cube.

  size_t pregenerated_count = handle.getPregeneratedCubesCount();

  parac_path l = parac_path_get_next_left(p);
  parac_path r = parac_path_get_next_right(p);
  size_t s = (handle.getNormalizedPathLength() - l.length);
  parac_path_type depth_shifted_l = parac_path_get_depth_shifted(l) << s;
  parac_path_type depth_shifted_r = parac_path_get_depth_shifted(r) << s;

  left = depth_shifted_l < pregenerated_count;
  right = depth_shifted_r < pregenerated_count;

  return left || right;
}

Supplied::Supplied() {}
Supplied::~Supplied() {}

CubeIteratorRange
Supplied::cube(parac_path p, CaDiCaLManager& mgr) {
  (void)p;
  (void)mgr;
  return CubeIteratorRange(m_cube.begin(), m_cube.end());
}
std::unique_ptr<Source>
PathDefined::copy() const {
  return std::make_unique<PathDefined>(*this);
}

bool
Supplied::split(parac_path p,
                CaDiCaLManager& mgr,
                const CaDiCaLHandle& handle,
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
                   const CaDiCaLHandle& handle,
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
Unspecified::copy() const {
  return std::make_unique<Unspecified>(*this);
}
}

CEREAL_REGISTER_TYPE(parac::solver::cubesource::PathDefined)
CEREAL_REGISTER_TYPE(parac::solver::cubesource::Supplied)
CEREAL_REGISTER_TYPE(parac::solver::cubesource::Unspecified)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::PathDefined)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::Supplied)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::Unspecified)
