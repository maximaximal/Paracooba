#ifndef PARACOOBA_MESSAGES_JOBDESCRIPTION_TRANSMITTER
#define PARACOOBA_MESSAGES_JOBDESCRIPTION_TRANSMITTER

#include <functional>

namespace paracooba {
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
