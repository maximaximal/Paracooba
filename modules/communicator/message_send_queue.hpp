#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include <paracooba/common/status.h>
#include <paracooba/common/types.h>

#include "transmit_mode.hpp"

struct parac_message;
struct parac_file;
struct parac_compute_node;

namespace parac::communicator {
class PacketHeader;
class Service;
class TCPConnection;

/** @brief MessageSendQueue is the central class handling the sending of
 * messages.
 *
 * This class is also the one that is embedded into compute nodes and that is
 * called from all other threads.
 */
class MessageSendQueue : public std::enable_shared_from_this<MessageSendQueue> {
  public:
  enum Event { MessagesAvailable, KillConnection };

  /** @brief Notifies the message consumer of new messages available in this
   * queue.
   *
   * @return True if consumer still exists, false if the callback should be
   * de-registered.
   */
  using NotificationFunc = std::function<bool(Event)>;

  struct EndTag {};
  struct ACKTag {};

  explicit MessageSendQueue(Service& service, parac_id remoteId);
  ~MessageSendQueue();

  void send(parac_message&& message);
  void send(parac_file&& file);
  void send(EndTag&& end);
  void send(parac_message& message);
  void send(parac_file& file);
  void send(EndTag& end);
  void sendACK(uint32_t id, parac_status status);

  static void static_send_message_to(parac_compute_node* compute_node,
                                     parac_message* msg);
  static void static_send_file_to(parac_compute_node* compute_node,
                                  parac_file* msg);
  static void static_compute_node_free_func(parac_compute_node* n);
  static bool static_available_to_send_to(parac_compute_node* n);

  void sendMessageTo(parac_compute_node& compute_node, parac_message& message);
  void sendFileTo(parac_compute_node& compute_node, parac_file& file);
  void computeNodeFreeFunc(parac_compute_node& compute_node);
  bool availableToSendTo(parac_compute_node& compute_node);

  bool handleACK(const PacketHeader& ack);

  using RefValueType =
    std::variant<parac_message*, parac_file*, ACKTag*, EndTag*>;

  struct EntryRef {
    PacketHeader* header = nullptr;
    RefValueType body;
    TransmitMode transmitMode;

    parac_message& message() {
      parac_message* m = std::get<parac_message*>(body);
      assert(m);
      return *m;
    }
    parac_file& file() {
      parac_file* f = std::get<parac_file*>(body);
      assert(f);
      return *f;
    }
  };

  /** @brief Get the topmost entry queued for sending.
   *
   * It is not removed from the internal queue! The entry reference is only
   * valid while popFromQueue was not called.
   */
  EntryRef front();

  /** @brief Pop the topmost entry after it was written to the network.
   *
   * Removes the entry from the queue, adds it into the waiting for ACK map and
   * sets the internal sent timestamp.
   */
  void popFromQueued();

  /** @brief Checks if the send queue is empty.
   *
   * Additionally maybe add new items to send queue if old entries were not
   * received yet. This makes consumers of items to send automatically re-send
   * items that are nearing time-out. This happens when a message is older than
   * it's half-time.
   */
  bool empty();

  /** @brief Registers a CB to be called when new messages should be sent or a
   * connection be killed.
   */
  std::pair<parac_compute_node*, std::shared_ptr<MessageSendQueue>>
  registerNotificationCB(const NotificationFunc& f,
                         const std::string& connectionString,
                         bool isConnectionInitiator);
  /** @brief Deregister the CB and make message counter in service go to 0 for
   * faster shutdown. */
  void deregisterNotificationCB();

  /** @brief Check contained messages for eventual resends.
   */
  void tick();

  /** @brief Called when any kind of communication happened on a connection
   * associated with this queue.
   *
   * This keeps the logical connection to the remote compute node alive.
   */
  void notifyOfRead() {
    m_lastHeardOfRemote = std::chrono::steady_clock::now();
  }

  private:
  struct Internal;
  struct Entry;
  using SentMap = std::map<uint32_t, Entry>;
  using SendQueue = std::queue<Entry>;

  Service& m_service;
  std::unique_ptr<Internal> m_internal;

  std::recursive_mutex m_queuedMutex;
  std::recursive_mutex m_waitingForACKMutex;

  parac_id m_remoteId = 0;
  uint32_t m_messageNumber = 0;
  uint32_t m_ACKdMessageCount = 0;
  parac_compute_node* m_remoteComputeNode = nullptr;

  std::atomic_size_t m_trackedQueueSize = 0;

  NotificationFunc m_notificationFunc;
  std::string m_connectionString;

  std::atomic_bool m_availableToSendTo = false;
  std::atomic_bool m_dropped = false;

  std::atomic_bool m_serviceKnowsAboutWrites = false;

  std::chrono::steady_clock::time_point m_lastHeardOfRemote =
    std::chrono::steady_clock::now();

  void send(Entry&& e, bool resend = false);

  void clear();
};
}
