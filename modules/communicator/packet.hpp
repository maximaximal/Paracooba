#pragma once

#include <cstdint>
#include <paracooba/common/message_kind.h>
#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

namespace parac::communicator {
struct PacketHeader {
  parac_id originator;
  uint32_t number;
  parac_message_kind kind;

  union {
    uint64_t size;
    parac_status ack_status;
  };
};

struct PacketFileHeader {
  char name[255];
};
}
