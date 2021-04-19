#include "cube_source.hpp"
#include "cadical_handle.hpp"
#include "cadical_manager.hpp"
#include "paracooba/solver/cube_iterator.hpp"
#include "solver_config.hpp"

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
                   CaDiCaLHandle& handle,
                   bool& left,
                   bool& right) {
  (void)mgr;

  if(handle.getNormalizedPathLength() <
     static_cast<size_t>(parac_path_length(p)) + 1) {
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

  if(parac_path_is_overlength(l) || parac_path_is_overlength(r)) {
    left = false;
    right = false;
    return false;
  }

  size_t s = (handle.getNormalizedPathLength() - parac_path_length(l));
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
                CaDiCaLHandle& handle,
                bool& left,
                bool& right) {
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
                   bool& right) {
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

CaDiCaLCubes::CaDiCaLCubes() = default;
CaDiCaLCubes::CaDiCaLCubes(Literal splittingLiteral,
                           uint32_t concurrentCubeTreeNumber)
  : m_splittingLiteral(splittingLiteral)
  , m_concurrentCubeTreeNumber(concurrentCubeTreeNumber) {}
CaDiCaLCubes::CaDiCaLCubes(Cube currentCube, uint32_t concurrentCubeTreeNumber)
  : m_currentCube(currentCube)
  , m_concurrentCubeTreeNumber(concurrentCubeTreeNumber) {}
CaDiCaLCubes::~CaDiCaLCubes() = default;

CubeIteratorRange
CaDiCaLCubes::cube(parac_path p, CaDiCaLManager& mgr) {
  (void)p;
  (void)mgr;
  if(m_currentCube.size() == 0) {
    return CubeIteratorRange();
  }
  return CubeIteratorRange(m_currentCube.begin(), m_currentCube.end());
}

bool
CaDiCaLCubes::split(parac_path p,
                    CaDiCaLManager& mgr,
                    CaDiCaLHandle& handle,
                    bool& left,
                    bool& right) {
  parac_log(PARAC_CUBER,
            PARAC_TRACE,
            "Splitting formula with originator {} from path {} in CaDiCaLCuber "
            "with splitting lit {} and current cube {} in cube tree number {}",
            mgr.config().OriginatorId(),
            p,
            m_splittingLiteral,
            fmt::join(m_currentCube, ", "),
            m_concurrentCubeTreeNumber);

  if(m_splittingLiteral != 0) {
    // Split was already decided previously! No other split has to be computed
    // locally.
    left = true;
    right = true;
    return true;
  } else {
    // Maybe a splitting literal has to be computed! This depends on the depth
    // of the current cube and the solver config.
    if(m_currentCube.size() < mgr.config().InitialMinimalCubeDepth()) {
      // Split again using resplit!
      auto r = handle.resplitCube(p, m_currentCube, mgr.config());
      assert(r.first);
      if(r.first != PARAC_SPLITTED) {
        left = false;
        right = false;
        return false;
      }
      assert(r.second != 0);
      m_splittingLiteral = r.second;
      left = true;
      right = true;
      return true;
    } else {
      // Desired depth reached! Now, solving (with potential resplitting)
      // starts.
      left = false;
      right = false;
      return false;
    }
  }
  left = false;
  right = false;
  return false;
}
std::unique_ptr<Source>
CaDiCaLCubes::copy() const {
  return std::make_unique<CaDiCaLCubes>(*this);
}

std::shared_ptr<Source>
CaDiCaLCubes::leftChild(std::shared_ptr<Source> selfSource) {
  auto self = std::dynamic_pointer_cast<CaDiCaLCubes>(selfSource);
  assert(self);
  std::shared_ptr<CaDiCaLCubes> copy = std::make_shared<CaDiCaLCubes>(*self);
  copy->m_currentCube.emplace_back(-copy->m_splittingLiteral);
  copy->m_splittingLiteral = 0;
  return copy;
}
std::shared_ptr<Source>
CaDiCaLCubes::rightChild(std::shared_ptr<Source> selfSource) {
  auto self = std::dynamic_pointer_cast<CaDiCaLCubes>(selfSource);
  assert(self);
  std::shared_ptr<CaDiCaLCubes> copy = std::make_shared<CaDiCaLCubes>(*self);
  copy->m_currentCube.emplace_back(copy->m_splittingLiteral);
  copy->m_splittingLiteral = 0;
  return copy;
}
}

CEREAL_REGISTER_TYPE(parac::solver::cubesource::PathDefined)
CEREAL_REGISTER_TYPE(parac::solver::cubesource::Supplied)
CEREAL_REGISTER_TYPE(parac::solver::cubesource::Unspecified)
CEREAL_REGISTER_TYPE(parac::solver::cubesource::CaDiCaLCubes)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::PathDefined)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::Supplied)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::Unspecified)
CEREAL_REGISTER_POLYMORPHIC_RELATION(parac::solver::cubesource::Source,
                                     parac::solver::cubesource::CaDiCaLCubes)
