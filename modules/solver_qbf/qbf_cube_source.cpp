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
                           bool& right,
                           bool& extended) {
  (void)handle;
  const Parser::Quantifiers& quantifiers = mgr.parser().quantifiers();
  const SolverConfig& cfg = mgr.config();

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

    // We must decide if we split on the literal that follows or if we do an
    // extended integer based split.
    if(cfg.integerBasedSplitsEnabled()) {
      size_t currentIntSplitsIndex =
        cfg.integerBasedSplitsCurrentIndex(m_currentCube.size());
      auto& intSplits = cfg.integerBasedSplits();
      if(currentIntSplitsIndex < intSplits.size()) {
        int intSplit = intSplits[currentIntSplitsIndex];
        int splitLength =
          cfg.integerBasedSplitsCurrentLength(currentIntSplitsIndex);

        // Only if the current layer should really be considered!
        if(intSplit > 0 &&
           m_currentCube.size() + splitLength < quantifiers.size()) {
          // Collect splitting literals.
          m_splittingLiterals.clear();
          m_splittingLiterals.reserve(splitLength);
          Parser::Quantifier::Type firstQuantiferType =
            quantifiers[m_currentCube.size()].type();
          for(size_t i = m_currentCube.size();
              i < m_currentCube.size() + splitLength;
              ++i) {
            Parser::Quantifier currentQuantifier = quantifiers[i];
            if(firstQuantiferType != currentQuantifier.type()) {
              parac_log(PARAC_SOLVER,
                        PARAC_LOCALERROR,
                        "Please check the provided integer splits for formula "
                        "with originator {}! Split {} with "
                        "parameter {} wraps over multiple quantifiers! "
                        "Proceeding as normal split.",
                        cfg.originatorId(),
                        currentIntSplitsIndex,
                        intSplit);
              m_splittingLiterals.clear();
              break;
            }
            m_splittingLiterals.emplace_back(currentQuantifier.alit());
          }

          if(m_splittingLiterals.size() > 0) {
            parac_log(PARAC_SOLVER,
                      PARAC_TRACE,
                      "Splitting formula with originator {} on path {} using "
                      "integer based splitting by {}. Literals: {}",
                      mgr.config().originatorId(),
                      p,
                      intSplit,
                      fmt::join(m_splittingLiterals, " "));
            m_splitCount = intSplit;

            left = false;
            right = false;
            extended = true;
            return true;
          }
        }
      }
    }

    m_splittingLiterals = { quantifiers[m_currentCube.size()].alit() };

    left = true;
    right = true;
    extended = false;
    return true;
  } else {
    // No more splitting possible, as no quantifiers are left.
    left = false;
    right = false;
    extended = false;
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
  assert(copy->m_splittingLiterals.size() == 1);
  copy->m_currentCube.emplace_back(-copy->m_splittingLiterals[0]);
  copy->m_splittingLiterals = { 0 };
  return copy;
}
std::shared_ptr<Source>
QuantifierTreeCubes::rightChild(std::shared_ptr<Source> selfSource) {
  auto self = std::dynamic_pointer_cast<QuantifierTreeCubes>(selfSource);
  assert(self);
  std::shared_ptr<QuantifierTreeCubes> copy =
    std::make_shared<QuantifierTreeCubes>(*self);
  assert(copy->m_splittingLiterals.size() == 1);
  copy->m_currentCube.emplace_back(copy->m_splittingLiterals[0]);
  copy->m_splittingLiterals = { 0 };
  return copy;
}

static inline Literal
makeLitNeg(Literal lit) {
  if(lit > 0)
    return -lit;
  return lit;
}
static inline Literal
makeLitPos(Literal lit) {
  if(lit < 0)
    return -lit;
  return lit;
}

static inline void
assignToSplittingLiterals(size_t n, Cube& s) {
  for(ssize_t i = s.size() - 1; i >= 0; --i) {
    size_t ni = s.size() - i - 1;
    if(((n >> ni) & 1u)) {
      s[i] = makeLitPos(s[i]);
    } else {
      s[i] = makeLitNeg(s[i]);
    }
  }
}

std::vector<std::shared_ptr<Source>>
QuantifierTreeCubes::children(std::shared_ptr<Source> selfSource) {
  std::vector<std::shared_ptr<Source>> children;
  children.reserve(m_splitCount);

  for(size_t split = 0; split < m_splitCount; ++split) {
    auto self = std::dynamic_pointer_cast<QuantifierTreeCubes>(selfSource);
    assert(self);
    std::shared_ptr<QuantifierTreeCubes> copy =
      std::make_shared<QuantifierTreeCubes>(*self);

    assignToSplittingLiterals(split, m_splittingLiterals);

    copy->m_currentCube.insert(copy->m_currentCube.end(),
                               m_splittingLiterals.begin(),
                               m_splittingLiterals.end());
    children.emplace_back(copy);
  }
  return children;
}
}

CEREAL_REGISTER_TYPE(parac::solver_qbf::cubesource::QuantifierTreeCubes)
CEREAL_REGISTER_POLYMORPHIC_RELATION(
  parac::solver_qbf::cubesource::Source,
  parac::solver_qbf::cubesource::QuantifierTreeCubes)
