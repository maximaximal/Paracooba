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
  virtual void transmitJobDescription(const JobDescription& jd,
                                      NetworkedNode* nn,
                                      std::function<void()> sendFinishedCB) = 0;
};
}
}

#endif
