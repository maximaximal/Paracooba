#pragma once

#include "paracooba/solver/cube_iterator.hpp"
#include <paracooba/solver/types.hpp>

#include <memory>
#include <variant>

struct parac_path;

namespace parac::solver {
class CaDiCaLManager;
class CaDiCaLHandle;

namespace cubesource {
class Source {
  public:
  Source() = default;
  ~Source() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr) = 0;
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const = 0;
  virtual const char* name() const = 0;
  virtual std::unique_ptr<Source> copy() const = 0;
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
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "PathDefined"; }
  virtual std::unique_ptr<Source> copy() const;
};

/** @brief Cube is supplied by parent. */
class Supplied : public Source {
  public:
  explicit Supplied(const Cube& cube)
    : m_cube(cube.begin(), cube.end()) {}
  ~Supplied();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "Supplied"; }
  virtual std::unique_ptr<Source> copy() const;

  private:
  const CubeIteratorRange m_cube;
};

/** @brief Cube is supplied by parent. */
class Unspecified : public Source {
  public:
  explicit Unspecified() = default;
  ~Unspecified() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "Unspecified"; }
  virtual std::unique_ptr<Source> copy() const;
};

using Variant = std::variant<cubesource::PathDefined,
                             cubesource::Supplied,
                             cubesource::Unspecified>;
}
}
