#include <chrono>
#include <type_traits>
#include <variant>

#include <paracooba/broker/broker.h>
#include <paracooba/common/compute_node.h>
#include <paracooba/common/compute_node_store.h>
#include <paracooba/common/file.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/message_kind.h>
#include <paracooba/module.h>

#include "file_sender.hpp"
#include "message_send_queue.hpp"
#include "packet.hpp"
#include "paracooba/common/status.h"
#include "service.hpp"
#include "tcp_connection.hpp"
#include "transmit_mode.hpp"

#ifdef ENABLE_DISTRAC
#include <distrac_paracooba.h>
#endif

namespace parac::communicator {
struct MessageSendQueue::Entry {
  Entry(const Entry& e) = default;
  Entry(Entry&& e) = default;

  Entry(parac_message& msg) noexcept
    : value(msg)
    , transmitMode(TransmitMode::Message) {
    header.kind = msg.kind;
    header.size = msg.length;
    header.originator = msg.originator_id;
  }
  Entry(parac_file& file) noexcept
    : value(file)
    , transmitMode(TransmitMode::File) {
    header.kind = PARAC_MESSAGE_FILE;
    header.size = FileSender::file_size(file.path);
    header.originator = file.originator;
  }
  Entry(EndTag& end) noexcept
    : value(end)
    , transmitMode(TransmitMode::End) {
    header.kind = PARAC_MESSAGE_END;
    header.size = 0;
  }
  Entry(parac_message&& msg) noexcept
    : value(std::move(msg))
    , transmitMode(TransmitMode::Message) {
    header.kind = msg.kind;
    header.size = msg.length;
    header.originator = msg.originator_id;
  }
  Entry(parac_file&& file) noexcept
    : value(std::move(file))
    , transmitMode(TransmitMode::File) {
    header.kind = PARAC_MESSAGE_FILE;
    header.size = FileSender::file_size(file.path);
    header.originator = file.originator;
  }
  Entry(EndTag&& end) noexcept
    : value(std::move(end))
    , transmitMode(TransmitMode::End) {
    header.kind = PARAC_MESSAGE_END;
    header.size = 0;
  }

  Entry(uint32_t id, parac_status status) noexcept
    : transmitMode(TransmitMode::ACK) {
    header.kind = PARAC_MESSAGE_ACK;
    header.ack_status = status;
    header.number = id;
    header.size = 0;
  }

  using value_type =
    std::variant<parac_message_wrapper, parac_file_wrapper, EndTag, ACKTag>;

  PacketHeader header;
  value_type value;
  TransmitMode transmitMode;
  std::chrono::time_point<std::chrono::steady_clock> queued =
    std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> sent;

  void applyToRefValue(RefValueType& v) {
    switch(value.index()) {
      case 0:
        v = &std::get<parac_message_wrapper>(value);
        break;
      case 1:
        v = &std::get<parac_file_wrapper>(value);
        break;
      case 2:
        v = &std::get<EndTag>(value);
        break;
      case 3:
        v = &std::get<ACKTag>(value);
        break;
    }
  }

  parac_message_wrapper& message() {
    return std::get<parac_message_wrapper>(value);
  }
  parac_file_wrapper& file() { return std::get<parac_file_wrapper>(value); }

  void operator()(parac_status status) {
    switch(transmitMode) {
      case TransmitMode::Message:
        message().doCB(status);
        break;
      case TransmitMode::File:
        file().doCB(status);
        break;
      default:
        break;
    }
  }

  ~Entry() {}
};

MessageSendQueue::MessageSendQueue(Service& service, parac_id remoteId)
  : m_service(service)
  , m_remoteId(remoteId) {
  static_assert(
    std::is_same_v<SentMap::key_type, decltype(PacketHeader::number)>);
}

MessageSendQueue::~MessageSendQueue() {
  // Notify all items that the message queue is no more
  while(!m_queued.empty()) {
    m_queued.front()(PARAC_CONNECTION_CLOSED);
    m_queued.pop();
  }
  for(auto& e : m_waitingForACK) {
    e.second(PARAC_CONNECTION_CLOSED);
  }
}

void
MessageSendQueue::send(parac_message&& message) {
  send(Entry(std::move(message)));
}
void
MessageSendQueue::send(parac_file&& file) {
  send(Entry(std::move(file)));
}
void
MessageSendQueue::send(EndTag&& end) {
  send(Entry(std::move(end)));
}
void
MessageSendQueue::send(parac_message& message) {
  send(Entry(message));
}
void
MessageSendQueue::send(parac_file& file) {
  send(Entry(file));
}
void
MessageSendQueue::send(EndTag& end) {
  send(Entry(end));
}
void
MessageSendQueue::sendACK(uint32_t id, parac_status status) {
  send(Entry(id, status));
}

static MessageSendQueue*
MessageSendQueueFromComputeNode(parac_compute_node* compute_node) {
  assert(compute_node);
  if(compute_node->communicator_userdata) {
    return static_cast<MessageSendQueue*>(compute_node->communicator_userdata);
  } else {
    return nullptr;
  }
}

void
MessageSendQueue::static_send_message_to(parac_compute_node* compute_node,
                                         parac_message* msg) {
  assert(msg);
  MessageSendQueue* self = MessageSendQueueFromComputeNode(compute_node);
  if(self)
    self->sendMessageTo(*compute_node, *msg);
  else
    msg->cb(msg, PARAC_CONNECTION_CLOSED);
}
void
MessageSendQueue::static_send_file_to(parac_compute_node* compute_node,
                                      parac_file* file) {
  assert(file);
  MessageSendQueue* self = MessageSendQueueFromComputeNode(compute_node);
  if(self)
    self->sendFileTo(*compute_node, *file);
  else
    file->cb(file, PARAC_CONNECTION_CLOSED);
}
void
MessageSendQueue::static_compute_node_free_func(
  parac_compute_node* compute_node) {
  MessageSendQueue* self = MessageSendQueueFromComputeNode(compute_node);
  if(self)
    self->computeNodeFreeFunc(*compute_node);
}
bool
MessageSendQueue::static_available_to_send_to(
  parac_compute_node* compute_node) {
  MessageSendQueue* self = MessageSendQueueFromComputeNode(compute_node);
  if(self)
    return self->availableToSendTo(*compute_node);
  return false;
}

void
MessageSendQueue::sendMessageTo(parac_compute_node& compute_node,
                                parac_message& message) {
  (void)compute_node;
  send(message);
}
void
MessageSendQueue::sendFileTo(parac_compute_node& compute_node,
                             parac_file& file) {
  (void)compute_node;
  send(file);
}
void
MessageSendQueue::computeNodeFreeFunc(parac_compute_node& compute_node) {
  // Data is freed by the service, because every MessageSendQueue is owned by
  // the service object and lives until the end of the program runtime. This
  // way, messages can be recovered if connections drop momentarily.
  compute_node.communicator_userdata = nullptr;
}
bool
MessageSendQueue::availableToSendTo(parac_compute_node& compute_node) {
  (void)compute_node;
  return m_availableToSendTo;
}

void
MessageSendQueue::send(Entry&& e, bool resend) {
  {
    std::unique_lock lock(m_queuedMutex);

    // ACKs already have a correct message number.
    if(e.header.kind != PARAC_MESSAGE_ACK && !resend) {
      e.header.number = m_messageNumber++;
    }

    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Queuing message of kind {} with id {} to remote {}",
              e.header.kind,
              e.header.number,
              m_remoteId);

    if(resend) {
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "The message of kind {} with id {} to remote {} is a resend! "
                "Last sent {}ms ago.",
                e.header.kind,
                e.header.number,
                m_remoteId,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - e.sent)
                  .count());
    }

    if(parac_message_kind_is_count_tracked(e.header.kind) && !resend) {
      ++m_trackedQueueSize;
    }

    m_queued.emplace(std::move(e));
  }

  if(m_availableToSendTo && m_notificationFunc) {
    if(!m_notificationFunc(MessagesAvailable)) {
      m_availableToSendTo = false;
    }
  } else {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALWARNING,
              "Cannot call conditionallyTriggerWriteHandler on TCPConnection "
              "for remote {} as none is known!",
              m_remoteId);
    m_availableToSendTo = false;
  }
}

bool
MessageSendQueue::handleACK(const PacketHeader& ack) {
  std::unique_lock lock(m_waitingForACKMutex);

  SentMap::iterator it;
  it = m_waitingForACK.find(ack.number);
  if(it == m_waitingForACK.end()) {
    return false;
  }

  auto& sentItem = it->second;
  try {
    sentItem(ack.ack_status);
    sentItem(PARAC_TO_BE_DELETED);
  } catch(...) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Exception while handling ACK number {} from remote {} for "
              "message kind {}!",
              ack.number,
              m_remoteId,
              it->second.header.kind);
  }

  ++m_ACKdMessageCount;

#ifdef ENABLE_DISTRAC
  auto distrac = m_service.handle().distrac;
  if(distrac) {
    parac_ev_send_msg_ack e{ m_remoteId,
                             sentItem.header.kind,
                             sentItem.header.number };
    distrac_push(distrac, &e, PARAC_EV_SEND_MSG_ACK);
  }
#endif

  parac_log(PARAC_COMMUNICATOR,
            PARAC_TRACE,
            "Received ACK number {} from node {} acknowledging that a "
            "message of kind {} successfully arrived.",
            ack.number,
            m_remoteId,
            it->second.header.kind);

  m_waitingForACK.erase(it);
  return true;
}

MessageSendQueue::EntryRef
MessageSendQueue::front() {
  std::unique_lock lock(m_queuedMutex);
  assert(!m_queued.empty());

  auto& front = m_queued.front();
  EntryRef r;
  r.header = &front.header;
  front.applyToRefValue(r.body);
  r.transmitMode = front.transmitMode;

  return r;
}

void
MessageSendQueue::popFromQueued() {
  auto r = [this]() {
    std::unique_lock lock(m_queuedMutex);
    auto r = m_queued.front();
    m_queued.pop();
    return r;
  }();

  auto kind = r.header.kind;
  auto number = r.header.number;

  r.sent = std::chrono::steady_clock::now();

  if(parac_message_kind_is_waiting_for_ack(kind)) {
    std::unique_lock lock(m_waitingForACKMutex);
    m_waitingForACK.try_emplace(number, std::move(r));

    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Sent message of kind {} with id {} to remote {} and waiting "
              "for ACK.",
              kind,
              number,
              m_remoteId);
  } else {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_TRACE,
              "Send message of kind {} with id {} to remote {} without "
              "waiting for ACK ",
              kind,
              number,
              m_remoteId);
  }

  if(parac_message_kind_is_count_tracked(kind)) {
    --m_trackedQueueSize;
  }
}

bool
MessageSendQueue::empty() {
  std::unique_lock lock(m_queuedMutex);
  return m_queued.empty();
}

std::pair<parac_compute_node*, std::shared_ptr<MessageSendQueue>>
MessageSendQueue::registerNotificationCB(const NotificationFunc& f,
                                         const std::string& connectionString,
                                         bool isConnectionInitiator) {
  if(m_notificationFunc) {
    // The connection was already established previously! It must now be decided
    // if the new connection should be used or the old one should be kept.

    bool idGreaterThanRemote = m_remoteId < m_service.handle().id;

    if(isConnectionInitiator && idGreaterThanRemote) {
      // Then it should be replaced if the queue was not already used.
      if(m_ACKdMessageCount >= 1) {
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_TRACE,
          "Cancel a connection to {} (connection string {}) even though it "
          "would have caused the other one to be killed, because "
          " the other active connection was already used and does not "
          "have to be canceled.",
          m_remoteId,
          connectionString);
        return { nullptr, nullptr };
      } else
        m_notificationFunc(KillConnection);
    } else if(!isConnectionInitiator && idGreaterThanRemote) {
      // The other connection should remain active.
      parac_log(
        PARAC_COMMUNICATOR,
        PARAC_TRACE,
        "Cancel a connection to {} (connection string {}) because "
        "local id {} > remote id {} and this is NOT the initiating side.",
        m_remoteId,
        connectionString,
        m_service.handle().id,
        m_remoteId);
      return { nullptr, nullptr };
    } else if(!isConnectionInitiator && !idGreaterThanRemote) {
      if(m_ACKdMessageCount >= 1) {
        // I'm the receiver of a more important connection. The other one should
        // be dropped. The same would happen on the other side.
        parac_log(
          PARAC_COMMUNICATOR,
          PARAC_TRACE,
          "Cancel a connection to {} (connection string {}) even though it "
          "would have caused the other one to be killed, because "
          " the other active connection was already used and does not "
          "have to be canceled.",
          m_remoteId,
          connectionString);
        return { nullptr, nullptr };
      } else
        m_notificationFunc(KillConnection);
    } else if(isConnectionInitiator && !idGreaterThanRemote) {
      // Again, the other already existing connection takes prevalence.
      parac_log(PARAC_COMMUNICATOR,
                PARAC_TRACE,
                "Cancel a connection to {} (connection string {}) because "
                "local id {} < remote id {} and this is the initiating side.",
                m_remoteId,
                connectionString,
                m_service.handle().id,
                m_remoteId);
      return { nullptr, nullptr };
    }
  }

  m_notificationFunc = f;

  m_availableToSendTo = true;

  m_connectionString = connectionString;

  if(m_remoteComputeNode) {
    m_remoteComputeNode->connection_string = m_connectionString.c_str();
    return { m_remoteComputeNode, shared_from_this() };
  }

  auto brokerMod = m_service.handle().modules[PARAC_MOD_BROKER];
  assert(brokerMod);
  auto broker = brokerMod->broker;
  assert(broker);
  auto compute_node_store = broker->compute_node_store;
  assert(compute_node_store);

  if(compute_node_store->has(compute_node_store, m_remoteId)) {
    parac_log(PARAC_COMMUNICATOR,
              PARAC_LOCALERROR,
              "Already has {} compute node in store!",
              m_remoteId);
  }
  assert(!compute_node_store->has(compute_node_store, m_remoteId));

  parac_compute_node* compute_node = compute_node_store->create_with_connection(
    compute_node_store,
    m_remoteId,
    &MessageSendQueue::static_compute_node_free_func,
    this,
    &MessageSendQueue::static_send_message_to,
    &MessageSendQueue::static_send_file_to,
    &MessageSendQueue::static_available_to_send_to);

  m_remoteComputeNode = compute_node;
  m_remoteComputeNode->connection_string = m_connectionString.c_str();

  return { compute_node, shared_from_this() };
}

void
MessageSendQueue::tick() {
  auto now = std::chrono::steady_clock::now();

  std::unique_lock l1(m_queuedMutex), l2(m_waitingForACKMutex);

  for(auto it = m_waitingForACK.begin(); it != m_waitingForACK.end(); ++it) {
    auto& e = it->second;

    if(std::chrono::duration_cast<std::chrono::milliseconds>(e.sent - now)
         .count() > m_service.messageTimeoutMS() / 2) {
      // Re-send the message in hope of it yet arriving on the other side.
      send(std::move(it->second), true);
      it = m_waitingForACK.erase(it);
    } else if(std::chrono::duration_cast<std::chrono::milliseconds>(e.sent -
                                                                    now)
                .count() > m_service.messageTimeoutMS()) {
      // The message ran into a timeout! Cancel that message and mark as never
      // received.
      if(parac_message_kind_is_count_tracked(it->second.header.kind)) {
        --m_trackedQueueSize;
      }
      it->second(PARAC_MESSAGE_TIMEOUT_ERROR);
      it->second(PARAC_TO_BE_DELETED);
      it = m_waitingForACK.erase(it);
      --m_trackedQueueSize;
    }
  }
}

}
