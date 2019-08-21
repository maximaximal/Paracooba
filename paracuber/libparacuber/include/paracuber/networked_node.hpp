#ifndef PARACUBER_NETWORKED_NODE_HPP
#define PARACUBER_NETWORKED_NODE_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace paracuber {
class NetworkedNode
{
  public:
  explicit NetworkedNode(boost::asio::ip::udp::endpoint remoteUdpEndpoint)
    : m_remoteUdpEndoint(remoteUdpEndpoint),
      m_remoteTcpEndoint(m_remoteUdpEndoint.address(), remoteUdpEndpoint.port())
  {
  }
  ~NetworkedNode() {}

  boost::asio::ip::udp::endpoint getRemoteUdpEndpoint()
  {
    return m_remoteUdpEndoint;
  }
  boost::asio::ip::tcp::endpoint getRemoteTcpEndpoint()
  {
    return m_remoteTcpEndoint;
  }

  void setRemoteUdpEndpoint(boost::asio::ip::udp::endpoint endpoint)
  {
    m_remoteUdpEndoint = endpoint;
  }
  void setRemoteTcpEndpoint(boost::asio::ip::tcp::endpoint endpoint)
  {
    m_remoteTcpEndoint = endpoint;
  }

  void setUdpPort(uint16_t p) { m_remoteUdpEndoint.port(p); }
  void setTcpPort(uint16_t p) { m_remoteTcpEndoint.port(p); }

  private:
  boost::asio::ip::udp::endpoint m_remoteUdpEndoint;
  boost::asio::ip::tcp::endpoint m_remoteTcpEndoint;
};
}

#endif
