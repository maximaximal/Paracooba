#ifndef PARACUBER_MESSAGES_JOB_INITIATOR
#define PARACUBER_MESSAGES_JOB_INITIATOR

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

namespace paracuber {
namespace messages {
class JobInitiator
{
  public:
  JobInitiator(CubingKind cubingKind)
    : cubingKind(cubingKind)
  {}
  ~JobInitiator() {}

  enum CubingKind
  {
    LiteralFrequency,
    PregeneratedCubes
  };

  CubingKind getCubingKind() const { return cubingKind; }

  using CubeMap = std::vector<int>;
  using JumpList = std::vector<size_t>;

  CubeMap& initCubeMap()
  {
    assert(getCubingKind() == PregeneratedCubes);
    return optionalCubeMap.emplace(std::move(CubeMap()));
  }
  const CubeMap& getCubeMap() const
  {
    assert(getCubingKind() == PregeneratedCubes);
    return optionalCubeMap.value();
  }
  size_t getCubeCount() const { return cubeCount; }

  /** @brief Create internal jump-list required for
   * CubingKind::PregeneratedCubes.
   *
   * Can be used with operator[] or getCube().
   * */
  void realise()
  {
    assert(getCubingKind() == PregeneratedCubes);
    assert(optionalCubeMap.has_value());
    assert(!optionalJumpList.has_value());

    auto& cubeMap = getCubeMap();
    auto jumpList = JumpList(cubeCount);
    std::generate(jumpList.begin(), jumpList.end(), [i = 0, cubeMap]() mutable {
      assert(cubeMap[i] == 0);
      size_t jumpPos = i;
      while(cubeMap[++i] != 0) {
      }
      return jumpPos;
    });

    optionalJumpList.emplace(std::move(jumpList));
  }

  const int* getCube(size_t i) const
  {
    assert(getCubingKind() == PregeneratedCubes);
    assert(optionalCubeMap.has_value());
    assert(optionalJumpList.has_value());
    auto& cubeMap = optionalCubeMap.value();
    auto& jumpList = optionalJumpList.value();
    assert(i < jumpList.size());
    return cubeMap.data() + jumpList[i];
  }
  const int* operator[](size_t i) const { return getCube(i); }

  private:
  friend class cereal::access;

  CubingKind cubingKind;
  std::optional<CubeMap> optionalCubeMap;
  std::optional<JumpList> optionalJumpList;
  size_t cubeCount;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(cubingKind),
       CEREAL_NVP(optionalCubeMap),
       CEREAL_NVP(cubeCount));
  }
};
}
}

#endif
