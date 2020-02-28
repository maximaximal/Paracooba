#ifndef PARACOOBA_MESSAGES_JOBDESCRIPTION_RECEIVER
#define PARACOOBA_MESSAGES_JOBDESCRIPTION_RECEIVER

#include <functional>

namespace paracooba {
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
