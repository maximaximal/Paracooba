#ifndef PARACOOBA_CLUSTER_NODE_STORE
#define PARACOOBA_CLUSTER_NODE_STORE

#include <boost/signals2/signal.hpp>

#include "messages/jobdescription_transmitter.hpp"
#include "types.hpp"

namespace paracooba {
class ClusterNode;

namespace messages {
class JobDescription;
}

class ClusterNodeStore : public messages::JobDescriptionTransmitter
{
  public:
  using ClusterNodeCreationPair = std::pair<ClusterNode&, bool>;
  using NodeFullyKnownSignal = boost::signals2::signal<void(ClusterNode&)>;

  virtual const ClusterNode& getNode(int64_t id) const = 0;
  virtual ClusterNode& getNode(int64_t id) = 0;
  virtual ClusterNodeCreationPair getOrCreateNode(ID id) = 0;
  virtual bool hasNode(ID id) const = 0;
  virtual void removeNode(int64_t id, const std::string& reason) = 0;

  virtual void nodeFullyKnown(ClusterNode& node)
  {
    m_nodeFullyKnownSignal(node);
  }

  NodeFullyKnownSignal& getNodeFullyKnownSignal()
  {
    return m_nodeFullyKnownSignal;
  }

  virtual void transmitJobDescription(messages::JobDescription&& jd,
                                      NetworkedNode& nn,
                                      SuccessCB sendFinishedCB);
  virtual void transmitJobDescription(messages::JobDescription&& jd,
                                      ID id,
                                      SuccessCB sendFinishedCB);

  virtual bool remoteConnectionStringKnown(
    const std::string& remoteConnectionString)
  {
    return false;
  }

  protected:
  NodeFullyKnownSignal m_nodeFullyKnownSignal;
};
}

#endif
