#ifndef PARACUBER_MESSAGES_JOB_PATH
#define PARACUBER_MESSAGES_JOB_PATH

#include <cstdint>
#include <optional>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

namespace paracuber {
namespace messages {
class JobPath
{
  public:
  using Path = int64_t;
  using TraceVector = std::vector<int>;

  JobPath() {}
  JobPath(Path path)
    : path(path)
  {}
  ~JobPath() {}

  /** @brief The path to work on.
   *
   * What work will be done will be decided based on the cubing algorithm used.
   */
  Path getPath() const { return path; };

  /** @brief Optional Trace of the path currently on.
   *
   * Existence depends on the Cubing Mechanism used. If the cubing mechanism is
   * != PregeneratedCubes, this trace is used. Else, it does not exist.
   */
  const TraceVector& getTrace() const { return optionalTrace.value(); }

  private:
  friend class cereal::access;

  Path path = 0;

  std::optional<TraceVector> optionalTrace;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(path), CEREAL_NVP(optionalTrace));
  }
};
}
}

#endif
