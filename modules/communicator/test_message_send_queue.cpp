#include <catch2/catch.hpp>

#include <paracooba/module.h>

#include <paracooba/broker/broker.h>
#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/message.h>

#include "message_send_queue.hpp"
#include "packet.hpp"
#include "paracooba/common/message_kind.h"
#include "service.hpp"

#include <cstdlib>
#include <cstring>

using namespace parac::communicator;

struct store {
  bool created[2] = { false, false };
  parac_compute_node_wrapper nodes[2];
};

static parac_compute_node*
store_get(parac_compute_node_store* s, parac_id id) {
  assert(s->userdata);
  store& st = *static_cast<store*>(s->userdata);
  if(id < 1 || id > 2)
    return nullptr;
  return st.created[id - 1] ? &st.nodes[id - 1] : nullptr;
}
static bool
store_has(parac_compute_node_store* store, parac_id id) {
  return store_get(store, id) != nullptr;
}

static parac_compute_node*
store_create_with_conn(
  parac_compute_node_store* s,
  parac_id id,
  parac_compute_node_free_func communicator_free,
  void* communicator_userdata,
  parac_compute_node_message_func send_message_func,
  parac_compute_node_file_func send_file_func,
  parac_compute_node_available_to_send_to_func available_to_send_to_func) {
  assert(s->userdata);
  store& st = *static_cast<store*>(s->userdata);
  REQUIRE(id >= 1);
  REQUIRE(id <= 2);
  REQUIRE(!st.created[id - 1]);
  st.created[id - 1] = true;
  parac_compute_node* n = store_get(s, id);
  n->communicator_free = communicator_free;
  n->communicator_userdata = communicator_userdata;
  n->send_message_to = send_message_func;
  n->send_file_to = send_file_func;
  n->available_to_send_to = available_to_send_to_func;
  return n;
}

struct environment {
  parac_handle handle;
  parac_module brokerMod;
  parac_module_broker broker;
  parac_compute_node_store pstore;
  store stor;
  Service service{ handle };
};

static void
setup_environment(environment& e, parac_id id) {
  memset(&e.handle, 0, sizeof(e.handle));
  memset(&e.brokerMod, 0, sizeof(e.brokerMod));
  memset(&e.broker, 0, sizeof(e.broker));
  memset(&e.pstore, 0, sizeof(e.pstore));

  e.handle.id = id;

  e.pstore.userdata = &e.stor;
  e.pstore.get = store_get;
  e.pstore.has = store_has;
  e.pstore.create_with_connection = store_create_with_conn;

  e.broker.compute_node_store = &e.pstore;
  e.brokerMod.broker = &e.broker;
  e.handle.modules[PARAC_MOD_BROKER] = &e.brokerMod;
}

TEST_CASE("Test MessageSendQueue send, notify and kill",
          "[communicator][message_send_queue]") {
  environment e;
  setup_environment(e, 1);

  std::shared_ptr<MessageSendQueue> sendQueue =
    std::make_shared<MessageSendQueue>(e.service, 2);

  bool firstCB = false;

  MessageSendQueue::NotificationFunc notificationCBs[] = {
    [&firstCB](MessageSendQueue::Event e) -> bool {
      switch(e) {
        case MessageSendQueue::MessagesAvailable:
          firstCB = true;
          break;
        case MessageSendQueue::KillConnection:
          firstCB = false;
          break;
      }
      return true;
    }
  };

  auto [cn, smq] =
    sendQueue->registerNotificationCB(notificationCBs[0], "", true);

  REQUIRE(cn);
  REQUIRE(!firstCB);

  parac_message_wrapper msg;
  msg.kind = PARAC_MESSAGE_NODE_STATUS;
  sendQueue->send(msg);

  REQUIRE(firstCB);

  {
    auto front = sendQueue->front();
    REQUIRE(front.header);
    REQUIRE(front.header->kind == PARAC_MESSAGE_NODE_STATUS);
    sendQueue->popFromQueued();
  }

  REQUIRE(sendQueue->empty());
}

TEST_CASE(
  "Test MessageSendQueue two-way connect special-case. Case Initiator;Receiver",
  "[communicator][message_send_queue]") {
  environment node1_e, node2_e;

  setup_environment(node1_e, 1);
  setup_environment(node2_e, 2);

  // Node 2 connects to node 1. Both nodes are daemon nodes and they try to
  // connect to each other in the exact same moment.
  //
  // This scenario leads to one connection being an initiator and the other
  // coming from the TCPAcceptor. The one from the acceptor must be closed while
  // the one from the initiator must remain open. This behavior is known on both
  // sides, so no other synchronization has to happen to restore the "only one
  // connection between nodes" invariant.

  std::shared_ptr<MessageSendQueue> node1_sq2 =
    std::make_shared<MessageSendQueue>(node1_e.service, 2);

  std::shared_ptr<MessageSendQueue> node2_sq1 =
    std::make_shared<MessageSendQueue>(node2_e.service, 1);

  bool node2_sq1_conn1_kill = false;
  bool node2_sq1_conn2_kill = false;

  bool node1_sq2_conn1_kill = false;
  bool node1_sq2_conn2_kill = false;

  {
    auto [node2_n1_cn, node2_sq1_ret] = node2_sq1->registerNotificationCB(
      [&node2_sq1_conn1_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node2_sq1_conn1_kill = true;
        return true;
      },
      "",
      true);
    auto [node1_n2_cn, node1_sq2_ret] = node1_sq2->registerNotificationCB(
      [&node1_sq2_conn1_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node1_sq2_conn1_kill = true;
        return true;
      },
      "",
      false);

    REQUIRE(node2_n1_cn);
    REQUIRE(node1_n2_cn);
  }

  {
    auto [node2_n1_cn, node2_sq1_ret] = node2_sq1->registerNotificationCB(
      [&node2_sq1_conn2_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node2_sq1_conn2_kill = true;
        return true;
      },
      "",
      false);
    auto [node1_n2_cn, node1_sq2_ret] = node1_sq2->registerNotificationCB(
      [&node1_sq2_conn2_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node1_sq2_conn2_kill = true;
        return true;
      },
      "",
      true);

    REQUIRE(!node2_n1_cn);
    REQUIRE(!node1_n2_cn);
  }

  REQUIRE(!node2_sq1_conn1_kill);
  // Never initiated anyways!
  REQUIRE(!node2_sq1_conn2_kill);

  REQUIRE(!node1_sq2_conn1_kill);
  REQUIRE(!node1_sq2_conn2_kill);
}

TEST_CASE(
  "Test MessageSendQueue two-way connect special-case. Case Receiver;Initiator",
  "[communicator][message_send_queue]") {
  environment node1_e, node2_e;

  setup_environment(node1_e, 1);
  setup_environment(node2_e, 2);

  // Node 2 connects to node 1. Both nodes are daemon nodes and they try to
  // connect to each other in the exact same moment.
  //
  // This scenario leads to one connection being an initiator and the other
  // coming from the TCPAcceptor. The one from the acceptor must be closed while
  // the one from the initiator must remain open. This behavior is known on both
  // sides, so no other synchronization has to happen to restore the "only one
  // connection between nodes" invariant.

  std::shared_ptr<MessageSendQueue> node1_sq2 =
    std::make_shared<MessageSendQueue>(node1_e.service, 2);

  std::shared_ptr<MessageSendQueue> node2_sq1 =
    std::make_shared<MessageSendQueue>(node2_e.service, 1);

  bool node2_sq1_conn1_kill = false;
  bool node2_sq1_conn2_kill = false;

  bool node1_sq2_conn1_kill = false;
  bool node1_sq2_conn2_kill = false;

  {
    auto [node2_n1_cn, node2_sq1_ret] = node2_sq1->registerNotificationCB(
      [&node2_sq1_conn1_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node2_sq1_conn1_kill = true;
        return true;
      },
      "",
      false);
    auto [node1_n2_cn, node1_sq2_ret] = node1_sq2->registerNotificationCB(
      [&node1_sq2_conn1_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node1_sq2_conn1_kill = true;
        return true;
      },
      "",
      true);

    REQUIRE(node2_n1_cn);
    REQUIRE(node1_n2_cn);
  }

  {
    auto [node2_n1_cn, node2_sq1_ret] = node2_sq1->registerNotificationCB(
      [&node2_sq1_conn2_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node2_sq1_conn2_kill = true;
        return true;
      },
      "",
      true);
    auto [node1_n2_cn, node1_sq2_ret] = node1_sq2->registerNotificationCB(
      [&node1_sq2_conn2_kill](MessageSendQueue::Event e) -> bool {
        if(e == MessageSendQueue::KillConnection)
          node1_sq2_conn2_kill = true;
        return true;
      },
      "",
      false);

    REQUIRE(node2_n1_cn);
    REQUIRE(node1_n2_cn);
  }

  REQUIRE(node2_sq1_conn1_kill);
  REQUIRE(!node2_sq1_conn2_kill);

  REQUIRE(node1_sq2_conn1_kill);
  REQUIRE(!node1_sq2_conn2_kill);
}
