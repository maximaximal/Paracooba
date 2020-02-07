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
  /** MUST MATCH CUBING KIND IN CNF! */
  enum CubingKind
  {
    LiteralFrequency,
    PregeneratedCubes
  };

  JobInitiator() {}
  JobInitiator(CubingKind cubingKind)
    : cubingKind(cubingKind)
  {}
  ~JobInitiator() {}

  CubingKind getCubingKind() const { return cubingKind; }

  using LiteralMap = std::vector<int>;
  using CubeMap = std::vector<int>;
  using JumpList = std::vector<size_t>;

  LiteralMap& initLiteralMap()
  {
    cubingKind = LiteralFrequency;
    return intVec;
  }
  CubeMap& initCubeMap()
  {
    cubingKind = PregeneratedCubes;
    return intVec;
  }
  const CubeMap& getCubeMap() const
  {
    assert(getCubingKind() == PregeneratedCubes);
    return intVec;
  }
  const LiteralMap& getLiteralMap() const
  {
    assert(getCubingKind() == LiteralFrequency);
    return intVec;
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
    assert(optionalJumpList.has_value());
    auto& jumpList = optionalJumpList.value();
    assert(i < jumpList.size());
    return intVec.data() + jumpList[i];
  }
  const int* operator[](size_t i) const { return getCube(i); }

  std::string tagline() const;

  private:
  friend class cereal::access;

  CubingKind cubingKind;
  std::vector<int> intVec;
  std::optional<JumpList> optionalJumpList;
  size_t cubeCount;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(cubingKind), CEREAL_NVP(intVec), CEREAL_NVP(cubeCount));

    // This immediately realises the jumplist on receive.
    if(cubingKind == PregeneratedCubes && !optionalJumpList.has_value())
      realise();
  }
};
}
}

#endif
