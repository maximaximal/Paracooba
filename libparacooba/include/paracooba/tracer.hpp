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
  JobPath,
  JobResult,
  JobInitiator
};
enum class TaskKind
{
  NotSet,
  DecisionTask,
  SolverTask
};

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
};

#define PARACOOBA_TRACER_LOG(TYPE)                                         \
  static void log(ID originId, const traceentry::TYPE& body)               \
  {                                                                        \
    using namespace std::chrono;                                           \
    auto& self = get();                                                    \
    if(self.m_active) {                                                    \
      self.logEntry(TraceEntry{                                            \
        self.m_thisId,                                                     \
        originId,                                                          \
        duration_cast<nanoseconds>(steady_clock::now() - self.m_startTime) \
          .count(),                                                        \
        traceentry::Kind::TYPE,                                            \
        traceentry::Body(body) });                                         \
    }                                                                      \
  }

class Tracer
{
  public:
  static Tracer& get()
  {
    static Tracer tracer;
    return tracer;
  }

  void setActivated(bool activated);
  void setOutputPath(const std::string_view& path);
  void setThisId(ID thisId);
  bool isActive() const { return m_active; }

  void logEntry(const TraceEntry& e);

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

  private:
  Tracer();
  ~Tracer();

  std::string m_outputPath = "";
  ID m_thisId = 0;
  bool m_active;

  std::chrono::time_point<std::chrono::steady_clock> m_startTime =
    std::chrono::steady_clock::now();

  struct OutHandle
  {
    std::string path;
    std::ofstream outStream;
  };

  static thread_local OutHandle m_outHandle;
};
}

#endif
