#include <catch2/catch.hpp>

#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"
#include <paracooba/common/messaging_queue.h>

TEST_CASE("Messaging Queue Push & Pop", "[commonc][messaging_queue]") {
  parac_messaging_queue queue;
  parac_messaging_queue_init(&queue);

  std::vector<parac_message> messages(PARAC_MESSAGING_QUEUE_SIZE + 1);

  messages[0].kind = PARAC_MESSAGE_PING;
  messages[1].kind = PARAC_MESSAGE_PONG;
  messages[PARAC_MESSAGING_QUEUE_SIZE].kind = PARAC_MESSAGE_JOB_PATH;

  paracooba::MessagingQueue cppQueue(queue);

  for(std::size_t i = 0; i < PARAC_MESSAGING_QUEUE_SIZE; ++i) {
    CAPTURE(i);
    REQUIRE(cppQueue.push(messages[i]) == PARAC_OK);
  }

  REQUIRE(cppQueue.push(messages[PARAC_MESSAGING_QUEUE_SIZE]) ==
          PARAC_QUEUE_FULL);

  parac_message* msg = cppQueue.pop();
  REQUIRE(msg->kind == PARAC_MESSAGE_PING);

  msg = cppQueue.pop();
  REQUIRE(msg->kind == PARAC_MESSAGE_PONG);

  REQUIRE(cppQueue.push(messages[PARAC_MESSAGING_QUEUE_SIZE]) == PARAC_OK);

  for(std::size_t i = 0; i < PARAC_MESSAGING_QUEUE_SIZE - 2; ++i) {
    REQUIRE(cppQueue.pop() != NULL);
  }

  msg = cppQueue.pop();
  REQUIRE(msg->kind == PARAC_MESSAGE_JOB_PATH);
}
