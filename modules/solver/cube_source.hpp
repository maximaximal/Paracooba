#pragma once

#include <boost/utility/base_from_member.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>

#include <paracooba/solver/types.hpp>
#include <paracooba/solver/cube_iterator.hpp>

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
                     const CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const = 0;
  virtual const char* name() const = 0;
  virtual std::unique_ptr<Source> copy() const = 0;

  // Also include serialize functions!
};

/** @brief Cube is defined by path and received from solver.
 *
 * Solver must have enough cubes in order to supply cube for this node in the
 * cube tree! */
class PathDefined : public Source {
  public:
  PathDefined();
  ~PathDefined();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     const CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "PathDefined"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive &ar) {
    ar(0);
  }
};

/** @brief Cube is supplied by parent. */
class Supplied : public Source {
  public:
  Supplied();
  explicit Supplied(const Cube& cube)
    : m_cube(cube) {}
  ~Supplied();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     const CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "Supplied"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive &ar) {
    ar(m_cube);
  }

  private:
  Cube m_cube;
};

/** @brief Cube is supplied by parent. */
class Unspecified : public Source {
  public:
  explicit Unspecified() = default;
  ~Unspecified() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     const CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) const;
  virtual const char* name() const { return "Unspecified"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive &ar) {
    ar(0);
  }
};

using Variant = std::variant<cubesource::PathDefined,
                             cubesource::Supplied,
                             cubesource::Unspecified>;
}
}
