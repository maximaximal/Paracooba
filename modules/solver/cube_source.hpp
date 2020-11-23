#pragma once

#include "paracooba/solver/cube_iterator.hpp"
#include <paracooba/solver/types.hpp>

#include <variant>

struct parac_path;

namespace parac::solver {
class CaDiCaLManager;

namespace cubesource {
class Source {
  public:
  Source() = default;
  ~Source() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr) = 0;
  virtual const char* name() const = 0;
};

/** @brief Cube is defined by path and received from solver.
 *
 * Solver must have enough cubes in order to supply cube for this node in the
 * cube tree! */
class PathDefined : public Source {
  public:
  PathDefined() = default;
  ~PathDefined() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual const char* name() const { return "PathDefined"; }
};

/** @brief Cube is supplied by parent. */
class Supplied : public Source {
  public:
  explicit Supplied(Cube cube)
    : m_cube(cube.begin(), cube.end()) {}
  ~Supplied();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual const char* name() const { return "Supplied"; }

  private:
  const CubeIteratorRange m_cube;
};

using Variant = std::variant<cubesource::PathDefined, cubesource::Supplied>;
}
}
