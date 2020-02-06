#ifndef PARACUBER_MESSAGES_JOBDESCRIPTION
#define PARACUBER_MESSAGES_JOBDESCRIPTION

#include <cereal/access.hpp>
#include <cereal/types/variant.hpp>
#include <cstdint>
#include <string>
#include <variant>

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
class NetworkedNode;

namespace messages {
class JobDescription
{
  public:
  JobDescription() {}
  JobDescription(int64_t originatorID)
    : originatorID(originatorID)
  {}
  ~JobDescription() {}

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
      return Kind::Initiator;
    return Unknown;
  }

  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobPath)
  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobResult)
  PARACUBER_MESSAGES_JOBDESCRIPTION_GETSET_BODY(JobInitiator)

  int64_t getOriginatorID() const { return originatorID; }

  using JobsVariant = std::variant<JobPath, JobResult, JobInitiator>;

  private:
  friend class cereal::access;

  int64_t originatorID;
  JobsVariant body;

  template<class Archive>
  void serialize(Archive& ar)
  {
    ar(CEREAL_NVP(originatorID), CEREAL_NVP(body));
  }
};

inline std::ostream&
operator<<(std::ostream& m, JobDescription::Kind kind)
{
  switch(kind) {
    case JobDescription::Kind::Path:
      return m << "Path";
    case JobDescription::Kind::Result:
      return m << "Result";
    case JobDescription::Kind::Initiator:
      return m << "Initiator";
    case JobDescription::Kind::Unknown:
      return m << "Unknown";
  }
  return m;
}
}
}

#endif
