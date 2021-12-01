#pragma once

#include <boost/utility/base_from_member.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/solver/cube_iterator.hpp>
#include <paracooba/solver/types.hpp>

#include <memory>
#include <variant>

#include "qbf_types.hpp"

struct parac_path;

namespace parac::solver_qbf {
class QBFSolverManager;
class GenericSolverHandle;
using CubeIteratorRange = solver::CubeIteratorRange;

namespace cubesource {
class Source {
  public:
  Source() = default;
  virtual ~Source() = default;

  virtual CubeIteratorRange cube(parac_path p, QBFSolverManager& mgr) = 0;
  virtual bool split(parac_path p,
                     QBFSolverManager& mgr,
                     GenericSolverHandle& handle,
                     bool& left,
                     bool& right,
                     bool& extended) = 0;
  virtual const char* name() const = 0;
  virtual std::unique_ptr<Source> copy() const = 0;

  virtual std::shared_ptr<Source> leftChild(std::shared_ptr<Source> self) {
    return self;
  };
  virtual std::shared_ptr<Source> rightChild(std::shared_ptr<Source> self) {
    return self;
  };

  virtual std::vector<std::shared_ptr<Source>> children(
    std::shared_ptr<Source> self) {
    return { leftChild(self), rightChild(self) };
  }

  virtual uint32_t prePathSortingCritereon() const { return 0; }

  // Also include serialize functions!
};

/** @brief The quantifier tree is used to generate cubes based on path. */
class QuantifierTreeCubes : public Source {
  public:
  explicit QuantifierTreeCubes();
  explicit QuantifierTreeCubes(Cube currentCube);
  explicit QuantifierTreeCubes(const QuantifierTreeCubes& o) = default;
  virtual ~QuantifierTreeCubes();

  virtual CubeIteratorRange cube(parac_path p, QBFSolverManager& mgr) override;

  /** @brief Traverse the quantifier tree provided by the QBF parser and look if
   * further splitting is possible. */
  virtual bool split(parac_path p,
                     QBFSolverManager& mgr,
                     GenericSolverHandle& handle,
                     bool& left,
                     bool& right,
                     bool& extended) override;
  virtual const char* name() const override { return "QuantifierTreeCubes"; }
  virtual std::unique_ptr<Source> copy() const override;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(m_currentCube);
    ar(m_splittingLiterals);
    ar(m_splitCount);
  }

  virtual std::shared_ptr<Source> leftChild(
    std::shared_ptr<Source> self) override;
  virtual std::shared_ptr<Source> rightChild(
    std::shared_ptr<Source> self) override;

  virtual std::vector<std::shared_ptr<Source>> children(
    std::shared_ptr<Source> self) override;

  private:
  Cube m_currentCube;
  Cube m_splittingLiterals;
  size_t m_splitCount = 0;
};
}
}
