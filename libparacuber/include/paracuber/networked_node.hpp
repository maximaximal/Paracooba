#ifndef PARACUBER_NETWORKED_NODE_HPP
#define PARACUBER_NETWORKED_NODE_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace paracuber {
class NetworkedNode
{
  public:
  explicit NetworkedNode(boost::asio::ip::udp::endpoint remoteUdpEndpoint,
                         int64_t id)
    : m_remoteUdpEndoint(remoteUdpEndpoint)
    , m_remoteTcpEndoint(m_remoteUdpEndoint.address(), remoteUdpEndpoint.port())
    , m_id(id)
  {}
  ~NetworkedNode() {}

  const boost::asio::ip::udp::endpoint& getRemoteUdpEndpoint() const
  {
    return m_remoteUdpEndoint;
  }
  const boost::asio::ip::tcp::endpoint& getRemoteTcpEndpoint() const
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
  int64_t getId() const { return m_id; }

  inline std::ostream& operator<<(std::ostream& o) { return o << getId(); }

  private:
  boost::asio::ip::udp::endpoint m_remoteUdpEndoint;
  boost::asio::ip::tcp::endpoint m_remoteTcpEndoint;
  int64_t m_id;
};
}

#endif
