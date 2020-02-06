#ifndef PARACUBER_MESSAGES_JOBDESCRIPTION_RECEIVER
#define PARACUBER_MESSAGES_JOBDESCRIPTION_RECEIVER

#include <functional>

namespace paracuber {
class NetworkedNode;

namespace messages {
class JobDescription;

class JobDescriptionReceiver
{
  public:
  virtual void receiveJobDescription(int64_t sentFromID,
                                     JobDescription&& jd) = 0;
};
}
}

#endif
