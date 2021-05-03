#pragma once

#include "paracooba/common/compute_node.h"
#include "paracooba/common/file.h"
#include "paracooba/common/message.h"
#include "paracooba/common/types.h"
#include <atomic>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>
#include <functional>
#include <optional>

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

struct parac_compute_node;
struct parac_message;
struct parac_file;
struct parac_module_solver_instance;
struct parac_handle;

namespace parac {
class NoncopyOStringstream;
class SpinLock;
}

namespace parac::broker {
class ComputeNodeStore;
class TaskStore;

class ComputeNode {
  public:
  ComputeNode(parac_compute_node& node,
              parac_handle& handle,
              ComputeNodeStore& store,
              TaskStore& taskStore);

  virtual ~ComputeNode();

  struct Description {
    std::string name;
    std::string host;
    uint32_t workers;
    uint16_t udpListenPort;
    uint16_t tcpListenPort;
    bool daemon = true;
    bool local = false;

    Description();
    Description(std::string name,
                std::string host,
                uint32_t workers,
                uint16_t udpListenPort,
                uint16_t tcpListenPort,
                bool daemon,
                bool local = false);

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(name),
         CEREAL_NVP(host),
         CEREAL_NVP(workers),
         CEREAL_NVP(udpListenPort),
         CEREAL_NVP(tcpListenPort),
         CEREAL_NVP(daemon));
    }

    void serializeToMessage(parac_message& msg) const;

    private:
    mutable std::unique_ptr<NoncopyOStringstream> m_descriptionStream;
  };

  struct SolverInstance {
    bool formula_received = false;
    bool formula_parsed = false;
    uint64_t workQueueSize = 0;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(formula_parsed), CEREAL_NVP(workQueueSize));
    }

    inline bool operator==(const SolverInstance& o) const noexcept {
      return formula_received == o.formula_received &&
             formula_parsed == o.formula_parsed &&
             workQueueSize == o.workQueueSize;
    }
  };

  struct Status {
    uint64_t workQueueSize() const;
    std::map<parac_id, SolverInstance> solverInstances;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(solverInstances));
    }

    void serializeToMessage(parac_message& msg) const;

    bool dirty() const { return m_dirty; }
    void resetDirty() const { m_dirty = false; }
    bool isParsed(parac_id id) const;

    static bool isDiffWorthwhile(const Status& s1, const Status& s2);

    Status() = default;
    Status(const Status& o);
    void operator=(const Status& o);
    bool operator==(const Status& o) const noexcept;

    void insertWorkerCount(uint32_t workers) const { m_workers = workers; };

    float computeUtilization() const;
    float computeFutureUtilization(uint64_t workQueueSize) const;

    private:
    mutable uint32_t m_workers = 0;

    mutable std::unique_ptr<NoncopyOStringstream> m_statusStream;

    friend class ComputeNode;
    mutable std::atomic_bool m_dirty = true;
    mutable std::atomic_flag m_writeFlag = ATOMIC_FLAG_INIT;
    mutable uint64_t m_cachedWorkQueueSize = 0;
  };

  const Description* description() const {
    return m_description ? &m_description.value() : nullptr;
  }
  std::pair<const Status&, SpinLock> status() const;
  bool isParsed(parac_id originator) const;
  float computeFutureUtilization(uint64_t workQueueSize) const;
  uint64_t workQueueSize() const;
  parac_id id() const;

  void incrementWorkQueueSize(parac_id originator);
  void decrementWorkQueueSize(parac_id originator);
  void formulaParsed(parac_id originator);

  void initDescription(const std::string& name,
                       const std::string& host,
                       uint32_t workers,
                       uint16_t udpListenPort,
                       uint16_t tcpListenPort,
                       bool demon,
                       bool local);

  void applyStatus(const Status& s);

  void receiveMessageFrom(parac_message& msg);
  void receiveMessageDescriptionFrom(parac_message& msg);
  void receiveMessageStatusFrom(parac_message& msg);
  void receiveMessageTaskResultFrom(parac_message& msg);
  void receiveMessageKnownRemotesFrom(parac_message& msg);
  void receiveMessageOfflineAnnouncement(parac_message& msg);

  void receiveFileFrom(parac_file& file);

  void runCBAfterParsingOfFormulaIsDone(std::function<void()> cb, parac_id id);

  float computeUtilization() const;

  parac_module_solver_instance* getSolverInstance();

  bool tryToOffloadTask();

  inline static bool compareByUtilization(const ComputeNode& first,
                                          const ComputeNode& second) {
    return first.computeUtilization() < second.computeUtilization();
  }

  void conditionallySendStatusTo(const Status& s);

  void notifyOfNewRemote(const parac_compute_node_wrapper& node);

  private:
  void sendKnownRemotes();
  void doNotifyOfNewRemotes();

  static void static_sendMessageTo(parac_compute_node* node,
                                   parac_message* msg);
  static void static_sendFileTo(parac_compute_node* node, parac_file* file);
  static void static_connectionDropped(parac_compute_node* node);
  static bool static_availableToSendTo(parac_compute_node* node);
  void sendMessageTo(parac_message& msg);
  void sendFileTo(parac_file& file);

  parac_compute_node& m_node;
  parac_handle& m_handle;
  ComputeNodeStore& m_store;
  TaskStore& m_taskStore;

  std::optional<Description> m_description;
  Status m_status;
  std::optional<Status> m_remotelyKnownLocalStatus;

  std::atomic_flag m_sendingStatusTo = ATOMIC_FLAG_INIT;
  mutable std::atomic_flag m_modifyingStatus = ATOMIC_FLAG_INIT;
  mutable std::shared_mutex m_descriptionMutex;

  std::unique_ptr<NoncopyOStringstream> m_knownRemotesOstream;

  std::atomic_flag m_sendingKnownRemotes = ATOMIC_FLAG_INIT;
  std::vector<parac_compute_node_wrapper::IDConnectionStringPair>
    m_newRemotesToNotifyAbout;

  std::mutex m_cbAfterParsingIsDoneMutex;
  std::map<parac_id, std::vector<std::function<void()>>> m_cbAfterParsingIsDone;
};

std::ostream&
operator<<(std::ostream& o, const ComputeNode::Description& d);
std::ostream&
operator<<(std::ostream& o, const ComputeNode::SolverInstance& si);
std::ostream&
operator<<(std::ostream& o, const ComputeNode::Status& s);
}
