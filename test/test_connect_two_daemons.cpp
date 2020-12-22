#include "paracooba/module.h"
#define PARAC_LOG_INCLUDE_FMT

#include "mocks.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"

#include "paracooba/common/log.h"

#include <atomic>
#include <catch2/catch.hpp>
#include <chrono>
#include <thread>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/message.h>

TEST_CASE("Connect two daemons.", "[integration,communicator,broker]") {
  ParacoobaMock master(1);

  auto master_cns = master.getBroker().compute_node_store;

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(!master_cns->has(master_cns, 2));

  auto n2 = master_cns->get(master_cns, 2);

  n2->receive_message_from = [](parac_compute_node* remote,
                                parac_message* msg) {
    assert(remote);
    assert(msg->data);
    if(msg->kind == PARAC_MESSAGE_NODE_STATUS) {
      assert(msg->length == 3);
      assert(msg->data[0] == 1);
      assert(msg->data[1] == 2);
      assert(msg->data[2] == 3);
    }
    msg->cb(msg, PARAC_OK);
  };

  ParacoobaMock daemon(2, nullptr, &master);
  auto daemon_cns = daemon.getBroker().compute_node_store;
  auto n1 = daemon_cns->get(daemon_cns, 1);
  n1->receive_message_from = n2->receive_message_from;
  REQUIRE(daemon_cns->has(daemon_cns, 2));

  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(master_cns->has(master_cns, 2));
  REQUIRE(daemon_cns->has(daemon_cns, 1));
  REQUIRE(daemon_cns->has(daemon_cns, 2));

  REQUIRE(n1->send_message_to);
  REQUIRE(n2->send_message_to);

  bool message_cb_called[3] = { false, false, false };

  parac_message_wrapper msg;
  char data[3] = { 1, 2, 3 };
  msg.data = data;
  msg.kind = PARAC_MESSAGE_NODE_STATUS;
  msg.length = 3;
  msg.origin = n1;
  msg.userdata = message_cb_called;
  msg.cb = [](parac_message* msg, parac_status status) {
    bool* cb_called = static_cast<bool*>(msg->userdata);
    if(!cb_called[0]) {
      cb_called[1] = status == PARAC_OK;
    } else {
      cb_called[2] = status == PARAC_TO_BE_DELETED;
    }
    cb_called[0] = true;
  };

  REQUIRE(!message_cb_called[0]);
  REQUIRE(!message_cb_called[1]);
  REQUIRE(!message_cb_called[2]);

  n1->send_message_to(n1, &msg);

  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  REQUIRE(message_cb_called[0]);
  REQUIRE(message_cb_called[1]);
  REQUIRE(message_cb_called[2]);

  std::atomic_int counter = 0;
  msg.kind = PARAC_MESSAGE_ONLINE_ANNOUNCEMENT;
  msg.userdata = &counter;
  msg.cb = [](parac_message* msg, parac_status status) {
    (void)status;
    std::atomic_int* counter = static_cast<std::atomic_int*>(msg->userdata);
    (*counter) += 1;
  };

  const size_t send_count = 20;

  for(size_t i = 0; i < send_count; ++i) {
    // n1->send_message_to(n1, &msg);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(15));

  // REQUIRE(counter == 2 * send_count);

  counter = 0;
  for(size_t i = 0; i < send_count; ++i) {
    n2->send_message_to(n2, &msg);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(15));

  REQUIRE(counter == 2 * send_count);
}
