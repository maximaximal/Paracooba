#ifndef PARACOOBA_TRACER_HPP
#define PARACOOBA_TRACER_HPP

#include <array>
#include <chrono>
#include <forward_list>
#include <fstream>
#include <string>

#include "types.hpp"

namespace paracooba {
namespace traceentry {
enum class MessageKind
{
  Unknown,
  CNF,
  OnlineAnnouncement,
  OfflineAnnouncement,
  AnnouncementRequest,
  NodeStatus,
  CNFTreeNodeStatusRequest,
  CNFTreeNodeStatusReply,
  NewRemoteConnected,
  Ping,
  Pong,
  JobPath,
  JobResult,
  JobInitiator
};
constexpr const char*
MessageKindToStr(MessageKind kind)
{
  switch(kind) {
    case MessageKind::Unknown:
      return "Unknown";
    case MessageKind::CNF:
      return "CNF";
    case MessageKind::OnlineAnnouncement:
      return "OnlineAnnouncement";
    case MessageKind::OfflineAnnouncement:
      return "OfflineAnnouncement";
    case MessageKind::AnnouncementRequest:
      return "AnnouncementRequest";
    case MessageKind::NodeStatus:
      return "NodeStatus";
    case MessageKind::CNFTreeNodeStatusRequest:
      return "CNFTreeNodeStatusRequest";
    case MessageKind::CNFTreeNodeStatusReply:
      return "CNFTreeNodeStatusReply";
    case MessageKind::NewRemoteConnected:
      return "NewRemoteConnected";
    case MessageKind::JobPath:
      return "JobPath";
    case MessageKind::JobResult:
      return "JobResult";
    case MessageKind::JobInitiator:
      return "JobInitiator";
    case MessageKind::Ping:
      return "Ping";
    case MessageKind::Pong:
      return "Pong";
  }
}
enum class TaskKind
{
  NotSet,
  DecisionTask,
  SolverTask
};
constexpr const char*
TaskKindToStr(TaskKind kind)
{
  switch(kind) {
    case TaskKind::NotSet:
      return "NotSet";
    case TaskKind::DecisionTask:
      return "DecisionTask";
    case TaskKind::SolverTask:
      return "SolverTask";
  }
}

struct ClientBegin
{
  int64_t timestamp;// System clock timestamp (seconds since epoch)
  bool sorted = false;
};
struct ComputeNodeDescription
{
  uint32_t workerCount = 0;
};
struct SendMsg
{
  ID target;
  uint64_t size;
  bool udp;
  MessageKind kind;
};
struct RecvMsg
{
  ID sender;
  uint64_t size;
  bool udp;
  MessageKind kind;
};
struct OffloadTask
{
  ID target;
  Path path;
  uint64_t localWorkQueueSize;
  uint64_t perceivedRemoteWorkQueueSize;
};
struct ReceiveTask
{
  ID source;
  Path path;
};
struct SendResult
{
  ID target;
  Path path;
  uint8_t state;// 0 = SAT, 1 = UNSAT, 2 = UNKNOWN
};
struct ReceiveResult
{
  ID source;
  Path path;
  uint8_t state;// 0 = SAT, 1 = UNSAT, 2 = UNKNOWN
};
struct StartProcessingTask
{
  uint32_t workerId;
  uint64_t localRealizedQueueSize;
  uint64_t localUnrealizedQueueSize;
  TaskKind kind;
  Path path;
};
struct FinishProcessingTask
{
  uint32_t workerId;
  uint64_t localRealizedQueueSize;
  uint64_t localUnrealizedQueueSize;
  TaskKind kind;
  Path path;
};
struct ConnectionEstablished
{
  ID remoteId;
  uint32_t ipv4 = 0;
  std::array<uint8_t, 16> ipv6 = { 0 };
  uint16_t remotePort;
};
struct ConnectionDropped
{
  ID remoteId;
  uint32_t ipv4 = 0;
  std::array<uint8_t, 16> ipv6 = { 0 };
  uint16_t remotePort;
};

enum class Kind
{
  ClientBegin,
  ComputeNodeDescription,
  SendMsg,
  RecvMsg,
  OffloadTask,
  ReceiveTask,
  SendResult,
  ReceiveResult,
  StartProcessingTask,
  FinishProcessingTask,
  ConnectionEstablished,
  ConnectionDropped
};
constexpr const char*
KindToStr(Kind kind)
{
  switch(kind) {
    case Kind::ClientBegin:
      return "ClientBegin";
    case Kind::ComputeNodeDescription:
      return "ComputeNodeDescription";
    case Kind::SendMsg:
      return "SendMsg";
    case Kind::RecvMsg:
      return "RecvMsg";
    case Kind::OffloadTask:
      return "OffloadTask";
    case Kind::ReceiveTask:
      return "ReceiveTask";
    case Kind::StartProcessingTask:
      return "StartProcessingTask";
    case Kind::FinishProcessingTask:
      return "FinishProcessingTask";
    case Kind::ConnectionEstablished:
      return "ConnectionEstablished";
    case Kind::ConnectionDropped:
      return "ConnectionDropped";
    case Kind::SendResult:
      return "SendResult";
    case Kind::ReceiveResult:
      return "ReceiveResult";
  }
}

#define PARACOOBA_TRACEENTRY_BODY_INIT(TYPE, MEMBER) \
  Body(const TYPE& MEMBER)                           \
    : MEMBER(MEMBER)                                 \
  {}

union Body
{
  traceentry::ClientBegin clientBegin;
  traceentry::ComputeNodeDescription computeNodeDescription;
  traceentry::SendMsg sendMsg;
  traceentry::RecvMsg recvMsg;
  traceentry::OffloadTask offloadTask;
  traceentry::ReceiveTask receiveTask;
  traceentry::SendResult sendResult;
  traceentry::ReceiveResult receiveResult;
  traceentry::StartProcessingTask startProcessingTask;
  traceentry::FinishProcessingTask finishProcessingTask;
  traceentry::ConnectionEstablished connectionEstablished;
  traceentry::ConnectionDropped connectionDropped;

  PARACOOBA_TRACEENTRY_BODY_INIT(ClientBegin, clientBegin)
  PARACOOBA_TRACEENTRY_BODY_INIT(ComputeNodeDescription, computeNodeDescription)
  PARACOOBA_TRACEENTRY_BODY_INIT(SendMsg, sendMsg)
  PARACOOBA_TRACEENTRY_BODY_INIT(RecvMsg, recvMsg)
  PARACOOBA_TRACEENTRY_BODY_INIT(OffloadTask, offloadTask)
  PARACOOBA_TRACEENTRY_BODY_INIT(ReceiveTask, receiveTask)
  PARACOOBA_TRACEENTRY_BODY_INIT(SendResult, sendResult)
  PARACOOBA_TRACEENTRY_BODY_INIT(ReceiveResult, receiveResult)
  PARACOOBA_TRACEENTRY_BODY_INIT(StartProcessingTask, startProcessingTask)
  PARACOOBA_TRACEENTRY_BODY_INIT(FinishProcessingTask, finishProcessingTask)
  PARACOOBA_TRACEENTRY_BODY_INIT(ConnectionEstablished, connectionEstablished)
  PARACOOBA_TRACEENTRY_BODY_INIT(ConnectionDropped, connectionDropped)
};
}

struct TraceEntry
{
  ID thisId = 0;
  ID originId = 0;
  int64_t nsSinceStart = 0;
  traceentry::Kind kind = traceentry::Kind::ClientBegin;
  traceentry::Body body;

  bool operator<(TraceEntry& o) { return nsSinceStart < o.nsSinceStart; }
};

inline std::ostream&
operator<<(std::ostream& o, const TraceEntry& e)
{
  return o << "ns=" + std::to_string(e.nsSinceStart) +
                " id=" + std::to_string(e.thisId) +
                " originId=" + std::to_string(e.originId) +
                " kind=" + traceentry::KindToStr(e.kind);
}

#define PARACOOBA_TRACER_LOG(TYPE)                           \
  static void log(ID originId, const traceentry::TYPE& body) \
  {                                                          \
    auto& self = get();                                      \
    if(self.m_active) {                                      \
      self.logEntry(TraceEntry{ self.m_thisId,               \
                                originId,                    \
                                self.getCurrentOffset(),     \
                                traceentry::Kind::TYPE,      \
                                traceentry::Body(body) });   \
    }                                                        \
  }

class Tracer
{
  public:
  static Tracer& get()
  {
    static Tracer tracer;
    return tracer;
  }

  void setActive(bool activated) { m_active = activated; }
  void setOutputPath(const std::string_view& path);
  void setThisId(ID thisId);
  bool isActive() const { return m_active; }

  void logEntry(const TraceEntry& e);

  static void resetStart(uint64_t offset);

  PARACOOBA_TRACER_LOG(ClientBegin)
  PARACOOBA_TRACER_LOG(ComputeNodeDescription)
  PARACOOBA_TRACER_LOG(SendMsg)
  PARACOOBA_TRACER_LOG(RecvMsg)
  PARACOOBA_TRACER_LOG(OffloadTask)
  PARACOOBA_TRACER_LOG(ReceiveTask)
  PARACOOBA_TRACER_LOG(SendResult)
  PARACOOBA_TRACER_LOG(ReceiveResult)
  PARACOOBA_TRACER_LOG(StartProcessingTask)
  PARACOOBA_TRACER_LOG(FinishProcessingTask)
  PARACOOBA_TRACER_LOG(ConnectionEstablished)
  PARACOOBA_TRACER_LOG(ConnectionDropped)

  inline int64_t getCurrentOffset()
  {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
             .count() -
           m_startTime;
  }

  private:
  Tracer();
  ~Tracer();

  std::string m_outputPath = "";
  ID m_thisId = 0;
  bool m_active = false;

  int64_t m_startTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();

  struct OutHandle
  {
    std::string path;
    std::ofstream outStream;
  };

  static thread_local OutHandle m_outHandle;
};
}

#endif
