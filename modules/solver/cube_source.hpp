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

struct parac_path;

namespace parac::solver {
class CaDiCaLManager;
class CaDiCaLHandle;

namespace cubesource {
class Source {
  public:
  Source() = default;
  virtual ~Source() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr) = 0;
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right) = 0;
  virtual const char* name() const = 0;
  virtual std::unique_ptr<Source> copy() const = 0;

  virtual std::shared_ptr<Source> leftChild(std::shared_ptr<Source> self) {
    return self;
  };
  virtual std::shared_ptr<Source> rightChild(std::shared_ptr<Source> self) {
    return self;
  };

  // Also include serialize functions!
};

/** @brief Cube is defined by path and received from solver.
 *
 * Solver must have enough cubes in order to supply cube for this node in the
 * cube tree! */
class PathDefined : public Source {
  public:
  PathDefined();
  virtual ~PathDefined();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right);
  virtual const char* name() const { return "PathDefined"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(0);
  }
};

/** @brief Cube is supplied by parent. */
class Supplied : public Source {
  public:
  Supplied();
  explicit Supplied(const Cube& cube)
    : m_cube(cube) {}
  virtual ~Supplied();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right);
  virtual const char* name() const { return "Supplied"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(m_cube);
  }

  private:
  Cube m_cube;
};

/** @brief Cube is supplied by parent. */
class Unspecified : public Source {
  public:
  explicit Unspecified() = default;
  virtual ~Unspecified() = default;

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right);
  virtual const char* name() const { return "Unspecified"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(0);
  }
};

/** @brief CaDiCaL is used to compute cubes on first split call. */
class CaDiCaLCubes : public Source {
  public:
  explicit CaDiCaLCubes();
  explicit CaDiCaLCubes(Literal splittingLiteral,
                        uint32_t concurrentCubeTreeNumber);
  explicit CaDiCaLCubes(Cube currentCube, uint32_t concurrentCubeTreeNumber);
  explicit CaDiCaLCubes(const CaDiCaLCubes& o) = default;
  virtual ~CaDiCaLCubes();

  virtual CubeIteratorRange cube(parac_path p, CaDiCaLManager& mgr);

  /** @brief First call to split runs CaDiCaL's splitting algorithm with
   * optionally applied starting lit. */
  virtual bool split(parac_path p,
                     CaDiCaLManager& mgr,
                     CaDiCaLHandle& handle,
                     bool& left,
                     bool& right);
  virtual const char* name() const { return "CaDiCaLCubes"; }
  virtual std::unique_ptr<Source> copy() const;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(m_currentCube, m_splittingLiteral, m_concurrentCubeTreeNumber);
  }

  virtual std::shared_ptr<Source> leftChild(std::shared_ptr<Source> self);
  virtual std::shared_ptr<Source> rightChild(std::shared_ptr<Source> self);

  uint32_t concurrentCubeTreeNumber() const {
    return m_concurrentCubeTreeNumber;
  }

  private:
  Cube m_currentCube;
  Literal m_splittingLiteral = 0;
  uint32_t m_concurrentCubeTreeNumber;
};

using Variant = std::variant<cubesource::PathDefined,
                             cubesource::Supplied,
                             cubesource::CaDiCaLCubes,
                             cubesource::Unspecified>;
}
}
