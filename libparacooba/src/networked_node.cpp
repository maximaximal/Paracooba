#include "../include/paracooba/networked_node.hpp"
#include "../include/paracooba/cluster-node.hpp"
#include "../include/paracooba/net/connection.hpp"
#include <boost/type_traits/is_stateless.hpp>
#include <memory>
#include <sstream>

namespace paracooba {
NetworkedNode::NetworkedNode(
  ID id,
  messages::MessageTransmitter& statelessMessageTransmitter,
  ClusterNode& node)
  : m_id(id)
  , m_statelessMessageTransmitter(statelessMessageTransmitter)
  , m_clusterNode(&node)
{}

NetworkedNode::~NetworkedNode()
{
  resetConnection();
}

void
NetworkedNode::transmitMessage(const messages::Message& msg,
                               NetworkedNode& nn,
                               SuccessCB sendFinishedCB)
{
  assert(&nn == this);
  if(m_connectionReadyWaiter.isReady()) {
    assert(m_connection);
    m_connection->sendMessage(msg, sendFinishedCB);
  } else {
    m_statelessMessageTransmitter.transmitMessage(msg, nn, sendFinishedCB);
  }
}

void
NetworkedNode::transmitJobDescription(messages::JobDescription&& jd,
                                      NetworkedNode& nn,
                                      SuccessCB sendFinishedCB)
{
  assert(&nn == this);
  m_connectionReadyWaiter.callWhenReady(
    [this, &jd, sendFinishedCB](net::Connection& conn) {
      conn.sendJobDescription(jd, sendFinishedCB);
    });
}

void
NetworkedNode::requestDeletion()
{
  m_deletionRequested = true;
  if(m_connection) {
    m_connection->exit();
    resetConnection();
  }
}

bool
NetworkedNode::isConnectionReady() const
{
  return m_connectionReadyWaiter.isReady();
}

bool
NetworkedNode::assignConnection(const net::Connection& conn)
{
  if(m_connection) {
    return false;
  }
  m_connection = std::make_unique<net::Connection>(conn);
  m_remoteTcpEndoint.address(conn.getRemoteTcpEndpoint().address());
  updateRemoteConnectionString();
  m_tcpEndpointSet = true;
  return true;
}

void
NetworkedNode::resetConnection()
{
  if(m_connection) {
    m_connection->resetRemoteNN();
  }
  m_connectionReadyWaiter.reset();
  m_connection.reset();
  removeActiveTCPClient();
}

void
NetworkedNode::updateRemoteConnectionString()
{
  std::stringstream ss;
  ss << m_remoteTcpEndoint;
  m_remoteConnectionString = ss.str();
}

std::ostream&
operator<<(std::ostream& o, const NetworkedNode& n)
{
  return o << n.getClusterNode();
}
}
