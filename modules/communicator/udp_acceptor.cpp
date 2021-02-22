#include <cassert>
#include <functional>
#include <limits>
#include <memory>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>

#include "cereal/archives/binary.hpp"
#include "cereal/details/helpers.hpp"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"
#include "service.hpp"
#include "udp_acceptor.hpp"
#include "udp_announcement.hpp"
#include <paracooba/broker/broker.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/log.h>
#include <paracooba/module.h>

namespace parac::communicator {

struct UDPAcceptor::Internal {
  Internal(Service& service)
    : service(service)
    , announcementTimer(service.ioContext())
    , socket(service.ioContext()) {}
  Service& service;

  boost::asio::steady_timer announcementTimer;

  boost::asio::ip::udp::socket socket;
  boost::asio::ip::udp::endpoint listenEndpoint;
  boost::asio::ip::udp::endpoint remoteEndpoint;
  boost::asio::ip::udp::endpoint broadcastEndpoint;

  std::string onlineAnnouncementBody;

  std::array<uint8_t, 4096> readBuf;
};

class Membuf : public std::basic_streambuf<char> {
  public:
  Membuf(const uint8_t* p, size_t l) { setg((char*)p, (char*)p, (char*)p + l); }
};
class Memstream : public std::istream {
  public:
  Memstream(const uint8_t* p, size_t l)
    : std::istream(&m_buffer)
    , m_buffer(p, l) {
    rdbuf(&m_buffer);
  }

  private:
  Membuf m_buffer;
};
// Source:
// https://tuttlem.github.io/2014/08/18/getting-istream-to-work-off-a-byte-array.html

UDPAcceptor::UDPAcceptor(const std::string& listenAddressStr,
                         uint16_t listenPort)
  : m_listenAddressStr(listenAddressStr)
  , m_listenPort(listenPort) {}

UDPAcceptor::~UDPAcceptor() {
  parac_log(PARAC_COMMUNICATOR, PARAC_DEBUG, "Destroy UDPAcceptor.");
}

parac_status
UDPAcceptor::start(Service& service) {
  m_internal = std::make_unique<Internal>(service);
  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Starting UDPAcceptor with listen address {} and port {}.",
            m_listenAddressStr,
            m_listenPort);

  boost::system::error_code ec;
#if BOOST_VERSION >= 106600
  auto address = boost::asio::ip::make_address_v4(m_listenAddressStr, ec);
#else
  auto address = boost::asio::ip::address_v4::from_string(m_listenAddressStr, ec);
#endif
  if(ec) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Cannot parse address {}! Error: {}",
              m_listenAddressStr,
              ec.message());
    return PARAC_INVALID_IP;
  }

  m_internal->listenEndpoint =
    boost::asio::ip::udp::endpoint(address, m_listenPort);

  m_internal->socket.open(m_internal->listenEndpoint.protocol());
  m_internal->socket.bind(m_internal->listenEndpoint, ec);

  m_internal->socket.set_option(boost::asio::socket_base::broadcast(true));
  //m_internal->socket.set_option(
  //  boost::asio::ip::udp::socket::reuse_address(true));

  if(ec) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Cannot bind to endpoint {}! Error: {}",
              m_internal->listenEndpoint,
              ec.message());
    return PARAC_BIND_ERROR;
  }

  accept();

  return PARAC_OK;
}

void
UDPAcceptor::startAnnouncements(const std::string& connectionString,
                                int32_t announcementIntervalMS) {
  m_announcementIntervalMS = announcementIntervalMS;

  boost::system::error_code ec;
#if BOOST_VERSION >= 106600
  auto address = boost::asio::ip::make_address_v4(
    m_internal->service.broadcastAddress(), ec);
#else
  auto address = boost::asio::ip::address_v4::from_string(
    m_internal->service.broadcastAddress(), ec);
#endif
  m_internal->broadcastEndpoint = boost::asio::ip::udp::endpoint(
    address, m_internal->service.udpTargetPort());

  parac_log(PARAC_COMMUNICATOR,
            PARAC_DEBUG,
            "Start UDP announcements every {}ms to endpoint {}.",
            announcementIntervalMS,
            m_internal->broadcastEndpoint);

  auto& body = m_internal->onlineAnnouncementBody;

  std::stringstream bodyOS;
  {
    cereal::BinaryOutputArchive oa(bodyOS);
    UDPAnnouncement a{ m_internal->service.id(), connectionString };
    oa(a);
  }
  body = bodyOS.str();

  announceAndScheduleNext(boost::system::error_code());
}

void
UDPAcceptor::announceAndScheduleNext(const boost::system::error_code& err) {
  if(!err) {
    announce();
    m_internal->announcementTimer.expires_from_now(
      std::chrono::milliseconds(m_announcementIntervalMS));
    m_internal->announcementTimer.async_wait(std::bind(
      &UDPAcceptor::announceAndScheduleNext, this, std::placeholders::_1));
  }
}

void
UDPAcceptor::readHandler(const ::boost::system::error_code& ec, size_t bytes) {
  if(ec) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error while reading from UDP! Remote endpoint: {}, bytes read: "
              "{}, error: {}",
              m_internal->remoteEndpoint,
              bytes,
              ec.message());
  } else {

    UDPAnnouncement announcement;

    try {
      {
        Memstream s(m_internal->readBuf.data(), bytes);
        cereal::BinaryInputArchive in(s);
        in(announcement);
      }

      // Check if this announcement should be ignored as the other node is
      // already known.
      auto& handle = m_internal->service.handle();
      assert(handle.modules);
      auto brokerMod = handle.modules[PARAC_MOD_BROKER];
      assert(brokerMod);
      auto broker = brokerMod->broker;
      auto compNodeStore = broker->compute_node_store;
      if(!compNodeStore->has(compNodeStore, announcement.id)) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_DEBUG,
          "Receive {} bytes over UDP from endpoint {} (ID {}), containing "
          "connection string {} which is now used to start a TCP connection.",
          bytes,
          m_internal->remoteEndpoint,
          announcement.id,
          announcement.tcpConnectionString);
        m_internal->service.connectToRemote(announcement.tcpConnectionString);
      }
    } catch(cereal::Exception& e) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Error during parsing of UDP message received from endpoint "
                "{}. Error: {}",
                m_internal->remoteEndpoint,
                e.what());
    }
  }

  accept();
}

#define STRBUF(SOURCE) boost::asio::buffer(SOURCE.data(), SOURCE.size())

void
UDPAcceptor::accept() {
  m_internal->socket.async_receive_from(
    STRBUF(m_internal->readBuf),
    m_internal->listenEndpoint,
    [this](auto& ec, auto b) { readHandler(ec, b); });
}

void
UDPAcceptor::announce() {
  boost::system::error_code ec;
  size_t sent =
    m_internal->socket.send_to(STRBUF(m_internal->onlineAnnouncementBody),
                               m_internal->broadcastEndpoint,
                               boost::asio::socket_base::message_flags(),
                               ec);
  if(ec) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Error {} while sending announcement to broadcast address {}! ",
              ec.message(),
              m_internal->broadcastEndpoint);
  } else {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Sent UDP announcement of {}B to endpoint {}.",
              sent,
              m_internal->broadcastEndpoint);
  }
}
}
