#ifndef PARACUBER_MESSAGES_JOB_RESULT
#define PARACUBER_MESSAGES_JOB_RESULT

#include <cstdint>
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

namespace paracuber {
namespace messages {
class JobResult
{
  public:
  using Path = uint64_t;
  using DataVec = std::vector<uint8_t>;

  enum State
  {
    SAT,
    UNSAT,
    UNKNOWN
  };

  JobResult() {}
  JobResult(Path path, State state)
    : path(path)
    , state(state)
  {}
  ~JobResult() {}

  Path getPath() const { return path; }
  State getState() const { return state; }
  DataVec& initDataVec() { return data.emplace(DataVec()); }
  DataVec& getDataVec() { return data.value(); }
  const DataVec& getDataVec() const { return data.value(); }

  std::string tagline() const;

  private:
  friend class cereal::access;

  Path path = 0;
  Path originPath = 0;
  State state = UNKNOWN;

  std::optional<DataVec> data;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(path), CEREAL_NVP(state), CEREAL_NVP(data));
  }
};

std::ostream&
operator<<(std::ostream& m, JobResult::State s);
}
}

#endif
