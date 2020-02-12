#include "paracuber/messages/online_announcement.hpp"
#include <catch2/catch.hpp>
#include <initializer_list>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <cereal/archives/binary.hpp>
#include <iostream>
#include <paracuber/messages/message.hpp>

using namespace boost;
using namespace paracuber::messages;

void
noop()
{}

TEST_CASE("Test sending serialised messages over UDP locally using boost asio")
{
  using boost::asio::ip::tcp;
  const int64_t origin = 123;

  Message msg = GENERATE(Message(origin).insert(OnlineAnnouncement()),
                         Message(origin).insert(AnnouncementRequest()));

  REQUIRE(msg.getType() != Type::NodeStatus);

  boost::asio::io_service io_service;

  tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 0));
  tcp::socket server_socket(io_service);
  tcp::socket client_socket(io_service);

  acceptor.async_accept(server_socket, boost::bind(&noop));
  client_socket.async_connect(acceptor.local_endpoint(), boost::bind(&noop));

  io_service.run();

  size_t size = 0;

  // Send
  {
    boost::asio::streambuf write_buffer;
    std::ostream output(&write_buffer);
    {
      cereal::BinaryOutputArchive oarchive(output);
      oarchive(msg);
    }
    size = write(server_socket, write_buffer.data());
  }

  // Receive
  {
    asio::streambuf read_buffer;
    auto bytes_transferred =
      asio::read(client_socket, read_buffer.prepare(size));
    read_buffer.commit(size);
    std::istream input(&read_buffer);
    cereal::BinaryInputArchive iarchive(input);
    Message receivedMsg;
    iarchive(receivedMsg);

    REQUIRE(receivedMsg.getOrigin() == origin);
    REQUIRE(receivedMsg.getType() == msg.getType());
  }
}
