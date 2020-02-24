#ifndef PARACUBER_MESSAGES_JOBDESCRIPTION_TRANSMITTER
#define PARACUBER_MESSAGES_JOBDESCRIPTION_TRANSMITTER

#include <functional>

namespace paracuber {
class NetworkedNode;

namespace messages {
class JobDescription;

class JobDescriptionTransmitter
{
  public:
  virtual void transmitJobDescription(
    JobDescription&& jd,
    int64_t id,
    std::function<void(bool)> sendFinishedCB) = 0;
};
}
}

#endif
