#include "qbf_cube_source.hpp"
#include "paracooba/solver/cube_iterator.hpp"
#include "parser_qbf.hpp"
#include "qbf_solver_manager.hpp"
#include "solver_qbf_config.hpp"

#include <paracooba/common/log.h>
#include <paracooba/common/path.h>

#include <cassert>

namespace parac::solver_qbf {
class QBFSolverManager;
}

namespace parac::solver_qbf::cubesource {
using solver::CubeIteratorRange;

QuantifierTreeCubes::QuantifierTreeCubes() = default;
QuantifierTreeCubes::QuantifierTreeCubes(Cube currentCube)
  : m_currentCube(currentCube) {}
QuantifierTreeCubes::~QuantifierTreeCubes() = default;

CubeIteratorRange
QuantifierTreeCubes::cube(parac_path p, QBFSolverManager& mgr) {
  (void)p;
  (void)mgr;
  if(m_currentCube.size() == 0) {
    return CubeIteratorRange();
  }
  return CubeIteratorRange(m_currentCube.begin(), m_currentCube.end());
}

bool
QuantifierTreeCubes::split(parac_path p,
                           QBFSolverManager& mgr,
                           GenericSolverHandle& handle,
                           bool& left,
                           bool& right) {
  (void)handle;
  const Parser::Quantifiers& quantifiers = mgr.parser().quantifiers();

  parac_log(PARAC_SOLVER,
            PARAC_TRACE,
            "Splitting formula with originator {} on path {} in "
            "QuantifierTreeCubes from current depth {} with max depth {}",
            mgr.config().originatorId(),
            p,
            m_currentCube.size(),
            quantifiers.size());

  if(m_currentCube.size() < quantifiers.size()) {
    // There still are some quantifiers left, so split!
    m_splittingLiteral = quantifiers[m_currentCube.size()].alit();

    left = true;
    right = true;
    return true;
  } else {
    // No more splitting possible, as no quantifiers are left.
    left = false;
    right = false;
    return false;
  }
}
std::unique_ptr<Source>
QuantifierTreeCubes::copy() const {
  return std::make_unique<QuantifierTreeCubes>(*this);
}

std::shared_ptr<Source>
QuantifierTreeCubes::leftChild(std::shared_ptr<Source> selfSource) {
  auto self = std::dynamic_pointer_cast<QuantifierTreeCubes>(selfSource);
  assert(self);
  std::shared_ptr<QuantifierTreeCubes> copy =
    std::make_shared<QuantifierTreeCubes>(*self);
  copy->m_currentCube.emplace_back(-copy->m_splittingLiteral);
  copy->m_splittingLiteral = 0;
  return copy;
}
std::shared_ptr<Source>
QuantifierTreeCubes::rightChild(std::shared_ptr<Source> selfSource) {
  auto self = std::dynamic_pointer_cast<QuantifierTreeCubes>(selfSource);
  assert(self);
  std::shared_ptr<QuantifierTreeCubes> copy =
    std::make_shared<QuantifierTreeCubes>(*self);
  copy->m_currentCube.emplace_back(copy->m_splittingLiteral);
  copy->m_splittingLiteral = 0;
  return copy;
}
}

CEREAL_REGISTER_TYPE(parac::solver_qbf::cubesource::QuantifierTreeCubes)
CEREAL_REGISTER_POLYMORPHIC_RELATION(
  parac::solver_qbf::cubesource::Source,
  parac::solver_qbf::cubesource::QuantifierTreeCubes)
