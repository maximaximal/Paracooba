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

  TraceVector& getTrace() { return trace; }
  const TraceVector& getTrace() const { return trace; }

  std::string tagline() const;

  private:
  friend class cereal::access;

  Path path = 0;
  TraceVector trace;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(path), CEREAL_NVP(trace));
  }
};
}
}

#endif
