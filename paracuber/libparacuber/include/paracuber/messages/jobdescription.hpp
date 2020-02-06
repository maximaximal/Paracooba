#ifndef PARACUBER_MESSAGES_JOBDESCRIPTION
#define PARACUBER_MESSAGES_JOBDESCRIPTION

#include <cstdint>
#include <string>
#include <variant>

#include <cereal/access.hpp>
#include <cereal/types/variant.hpp>

#include "job_initiator.hpp"
#include "job_path.hpp"
#include "job_result.hpp"

#define PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(NAME)      \
  const NAME& get##NAME() const { return std::get<NAME>(body); } \
  JobDescription insert##NAME(const NAME& val)                   \
  {                                                              \
    body = val;                                                  \
    return *this;                                                \
  }                                                              \
  JobDescription insert##NAME(const NAME&& val)                  \
  {                                                              \
    body = std::move(val);                                       \
    return *this;                                                \
  }                                                              \
  JobDescription insert(const NAME& val)                         \
  {                                                              \
    body = val;                                                  \
    return *this;                                                \
  }                                                              \
  JobDescription insert(const NAME&& val)                        \
  {                                                              \
    body = std::move(val);                                       \
    return *this;                                                \
  }

namespace paracuber {
namespace messages {
class JobDescription
{
  public:
  JobDescription();
  ~JobDescription();

  enum Kind
  {
    Path,
    Result,
    Initiator,
    Unknown
  };

  Kind getKind()
  {
    if(std::holds_alternative<JobPath>(body))
      return Kind::Path;
    if(std::holds_alternative<JobResult>(body))
      return Kind::Result;
    if(std::holds_alternative<JobInitiator>(body))
      return Kind::JobInitiator;
    return Unknown;
  }

  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobPath)
  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobResult)
  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobInitiator)

  private:
  friend class cereal::access;

  using JobsVariant = std::variant<JobPath, JobResult, JobInitiator>;
  JobsVariant body;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(body));
  }
};
}
}

#endif
