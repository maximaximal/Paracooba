#ifndef PARACUBER_MESSAGES_JOB_RESULT
#define PARACUBER_MESSAGES_JOB_RESULT

#include <cstdint>
#include <string>
#include <vector>

#include <cereal/access.hpp>

namespace paracuber {
namespace messages {
class JobResult
{
  public:
  JobResult();
  ~JobResult();

  using Path = int64_t;

  Path getPath() const { return path; }

  private:
  friend class cereal::access;

  Path path;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(path));
  }
};
}
}

#endif
