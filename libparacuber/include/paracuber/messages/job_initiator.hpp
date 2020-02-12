#ifndef PARACUBER_MESSAGES_JOB_INITIATOR
#define PARACUBER_MESSAGES_JOB_INITIATOR

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/variant.hpp>
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
  ~JobInitiator() {}

  using AllowanceMap = std::vector<int>;
  using CubeMap = std::map<uint64_t, int>;
  using DataVariant = std::variant<AllowanceMap, CubeMap>;

  CubingKind getCubingKind() const
  {
    if(std::holds_alternative<CubeMap>(body))
      return PregeneratedCubes;
    if(std::holds_alternative<AllowanceMap>(body))
      return LiteralFrequency;

    return PregeneratedCubes;
  }

  AllowanceMap& initAllowanceMap()
  {
    body = std::move(AllowanceMap());
    return getAllowanceMap();
  }
  AllowanceMap& getAllowanceMap()
  {
    assert(getCubingKind() == LiteralFrequency);
    return std::get<AllowanceMap>(body);
  }
  const AllowanceMap& getAllowanceMap() const
  {
    assert(getCubingKind() == LiteralFrequency);
    return std::get<AllowanceMap>(body);
  }
  CubeMap& initCubeMap()
  {
    body = std::move(CubeMap());
    return getCubeMap();
  }
  CubeMap& getCubeMap()
  {
    assert(getCubingKind() == PregeneratedCubes);
    return std::get<CubeMap>(body);
  }
  const CubeMap& getCubeMap() const
  {
    assert(getCubingKind() == PregeneratedCubes);
    return std::get<CubeMap>(body);
  }
  size_t getCubeCount() const { return getCubeMap().size(); }

  /** @brief Create cube map for
   * CubingKind::PregeneratedCubes mode.
   *
   * Can be used with getDecisionOnPath().
   * */
  size_t realise(const std::vector<int>& flatCubeArr);

  int getDecisionOnPath(int64_t path) const
  {
    assert(getCubingKind() == PregeneratedCubes);
    auto map = getCubeMap();
    auto it = map.find(path);
    return it == map.end() ? 0 : it->second;
  }

  std::string tagline() const;

  private:
  friend class cereal::access;

  DataVariant body;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(body));
  }
};
}
}

#endif
