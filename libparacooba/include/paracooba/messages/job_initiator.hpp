#ifndef PARACOOBA_MESSAGES_JOB_INITIATOR
#define PARACOOBA_MESSAGES_JOB_INITIATOR

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

namespace paracooba {
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

  struct PregeneratedCubesTag
  {};

  using AllowanceMap = std::vector<int>;
  using DataVariant = std::variant<AllowanceMap, PregeneratedCubesTag>;

  CubingKind getCubingKind() const
  {
    if(std::holds_alternative<PregeneratedCubesTag>(body))
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

  void initAsPregenCubes() { body = PregeneratedCubesTag{}; }

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
