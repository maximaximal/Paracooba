#include "../../include/paracuber/messages/node.hpp"
#include "../../include/paracuber/networked_node.hpp"
#include <cassert>

namespace paracuber {
namespace messages {

template<typename T>
void
ipv6AddressToUint64(T ip, uint64_t outBytes[2])
{
  auto bytes = ip.to_bytes();
  uint64_t ipuint64[2] = { *reinterpret_cast<uint64_t*>(&bytes[0]),
                           *reinterpret_cast<uint64_t*>(
                             &bytes[sizeof(uint64_t)]) };
  outBytes[0] = ipuint64[0];
  outBytes[1] = ipuint64[1];
}

void
Node::addKnownPeerFromNetworkedNode(NetworkedNode* nn)
{
  assert(nn);
  auto ip = nn->getRemoteUdpEndpoint().address();
  if(ip.is_v4()) {
    auto ipv4 = ip.to_v4();
    addKnownPeer(
      ipv4.to_ulong(), nn->getRemoteUdpEndpoint().port(), nn->getId());
  }
  if(ip.is_v6()) {
    auto ipv6 = ip.to_v6();
    uint64_t ipuint64[2];
    ipv6AddressToUint64(ipv6, ipuint64);
    addKnownPeer(ipuint64, nn->getRemoteUdpEndpoint().port(), nn->getId());
  }
}
bool
Node::peerLocallyReachable(NetworkedNode* from, const KnownPeer& to)
{
  assert(from);
  auto ip = from->getRemoteUdpEndpoint().address();

  if(ip.is_v4()) {
    auto ipv4 = ip.to_v4();
    uint32_t netmask = boost::asio::ip::address_v4::netmask(ipv4).to_ulong();
    uint32_t targetAddress = to.ipAddress[1];
    return (targetAddress & netmask) == netmask;
  }
  if(ip.is_v6()) {
    // TODO: Support IPV6
    assert(false);
    return false;
  }
  return false;
}
}
}
