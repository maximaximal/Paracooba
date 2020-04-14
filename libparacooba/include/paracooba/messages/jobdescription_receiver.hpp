#ifndef PARACOOBA_MESSAGES_JOBDESCRIPTION_RECEIVER
#define PARACOOBA_MESSAGES_JOBDESCRIPTION_RECEIVER

#include <memory>

namespace paracooba {
class NetworkedNode;

namespace messages {
class JobDescription;

class JobDescriptionReceiver
{
  public:
  virtual void receiveJobDescription(JobDescription&& jd,
                                     std::shared_ptr<NetworkedNode> nn) = 0;
};

/** @brief Provide a JobDescriptionReceiver instance to handle incoming job
 * descriptions. */
class JobDescriptionReceiverProvider
{
  public:
  virtual JobDescriptionReceiver* getJobDescriptionReceiver(
    int64_t subject) = 0;
};
}
}

#endif
