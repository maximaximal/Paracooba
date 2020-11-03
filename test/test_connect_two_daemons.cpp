#include "mocks.hpp"
#include "paracooba/common/log.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"

#include <catch2/catch.hpp>
#include <chrono>
#include <thread>

#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/message.h>

TEST_CASE("Connect two daemons.", "[integration,communicator,broker]") {
  ParacoobaMock master(1);
  ParacoobaMock daemon(2, &master);

  auto master_cns = master.getBroker().compute_node_store;
  auto daemon_cns = daemon.getBroker().compute_node_store;

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(!master_cns->has(master_cns, 2));
  REQUIRE(!daemon_cns->has(daemon_cns, 1));
  REQUIRE(daemon_cns->has(daemon_cns, 2));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  REQUIRE(master_cns->has(master_cns, 1));
  REQUIRE(master_cns->has(master_cns, 2));
  REQUIRE(daemon_cns->has(daemon_cns, 1));
  REQUIRE(daemon_cns->has(daemon_cns, 2));

  auto n1 = daemon_cns->get(daemon_cns, 1);
  auto n2 = master_cns->get(master_cns, 2);

  REQUIRE(n1->send_message_to);
  REQUIRE(n2->send_message_to);

  bool message_cb_called[3] = {false, false, false};

  parac_message_wrapper msg;
  char data[3] = { 1, 2, 3 };
  msg.data = data;
  msg.kind = PARAC_MESSAGE_NODE_STATUS;
  msg.length = 3;
  msg.origin = 1;
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

  n2->receive_message_from = [](parac_compute_node* remote,
                                parac_message* msg) {
    (void)remote;
    msg->cb(msg, PARAC_OK);
  };

  REQUIRE(!message_cb_called[0]);
  REQUIRE(!message_cb_called[1]);
  REQUIRE(!message_cb_called[2]);

  n1->send_message_to(n1, &msg);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  REQUIRE(message_cb_called[0]);
  REQUIRE(message_cb_called[1]);
  REQUIRE(message_cb_called[2]);
}
