#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/assignment_serializer.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/cuber/registry.hpp"
#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/task_factory.hpp"
#include "paracuber/messages/job_initiator.hpp"
#include "paracuber/messages/job_path.hpp"
#include "paracuber/messages/job_result.hpp"
#include "paracuber/messages/jobdescription.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/log/expressions/formatters/c_decorator.hpp>
#include <cadical/cadical.hpp>
#include <iostream>
#include <shared_mutex>
#include <vector>

extern "C"
{
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace paracuber {
CNF::CNF(ConfigPtr config,
         LogPtr log,
         int64_t originId,
         std::string_view dimacsFile)
  : m_config(config)
  , m_originId(originId)
  , m_dimacsFile(dimacsFile)
  , m_log(log)
  , m_logger(log->createLogger("CNF"))
  , m_cnfTree(std::make_unique<CNFTree>(config, originId))
{
  if(dimacsFile != "") {
    struct stat statbuf;
    int result = stat(m_dimacsFile.c_str(), &statbuf);
    if(result == -1) {
      PARACUBER_LOG(m_logger, Fatal)
        << "Could not find file \"" << dimacsFile << "\"!";
      return;
    }

    m_fileSize = statbuf.st_size;
    m_fd = open(m_dimacsFile.c_str(), O_RDONLY);
    assert(m_fd > 0);
  }

  connectToCNFTreeSignal();
  m_jobDescriptionTransmitter = config->getCommunicator();
}
CNF::CNF(const CNF& o)
  : m_config(o.m_config)
  , m_originId(o.m_originId)
  , m_dimacsFile(o.m_dimacsFile)
  , m_log(o.m_log)
  , m_logger(o.m_log->createLogger("CNF"))
  , m_cnfTree(std::make_unique<CNFTree>(o.m_config, o.m_originId))
  , m_jobDescriptionTransmitter(o.m_jobDescriptionTransmitter)
{
  connectToCNFTreeSignal();
}

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket, SendFinishedCB cb, bool first)
{
  SendDataStruct* data = &m_sendData[socket];

  if(first) {
    data->cb = cb;
    data->offset = 0;

    // Send the filename, so that the used compression algorithm can be known.
    // Also transfer the terminating \0.
    boost::filesystem::path p(m_dimacsFile);
    std::string dimacsBasename = p.filename().string();
    socket->write_some(
      boost::asio::buffer(dimacsBasename.c_str(), dimacsBasename.size() + 1));

    first = false;
  }

  // This is the root CNF, send over the file directly using the sendfile()
  // syscall.
  int ret = sendfile(socket->native_handle(), m_fd, &data->offset, m_fileSize);
  if(ret == -1) {
    std::cerr << "ERROR DURING SENDFILE: " << strerror(errno) << std::endl;
    m_sendData.erase(socket);
  }

  socket->async_write_some(boost::asio::null_buffers(),
                           std::bind(&CNF::sendCB, this, data, socket));
}

void
CNF::sendAllowanceMap(NetworkedNode* nn, SendFinishedCB finishedCallback)
{
  // First, wait for the allowance map to be ready.
  rootTaskReady.callWhenReady([this, nn, finishedCallback](CaDiCaLTask& ptr) {
    // Registry is only initialised after the root task arrived.
    getCuberRegistry().allowanceMapWaiter.callWhenReady(
      [this, nn, finishedCallback](cuber::Registry::AllowanceMap& map) {
        // This indirection is required to make this work from worker threads.
        // The allowance map may take a while to generate.
        messages::JobDescription jd(m_originId);

        switch(m_cuberRegistry->getCuberMode()) {
          case cuber::Registry::LiteralFrequency: {
            assert(map.size() > 0);
            auto initiator = messages::JobInitiator();
            initiator.initAllowanceMap() = map;
            jd.insert(initiator);
            break;
          }
          case cuber::Registry::PregeneratedCubes: {
            auto ji = m_cuberRegistry->getJobInitiator();
            assert(ji);
            jd.insert(*ji);
            break;
          }
        }

        m_jobDescriptionTransmitter->transmitJobDescription(
          std::move(jd), nn, finishedCallback);
      });
  });
}

void
CNF::sendPath(NetworkedNode* nn, CNFTree::Path p, SendFinishedCB finishedCB)
{
  assert(CNFTree::getDepth(p) > 0);
  assert(rootTaskReady.isReady());

  messages::JobPath jp(p);

  jp.getTrace().reserve(CNFTree::getDepth(p) - 1);
  std::shared_lock lock(m_cnfTreeMutex);
  m_cnfTree->writePathToLiteralContainer(
    jp.getTrace(), CNFTree::setDepth(p, CNFTree::getDepth(p) - 1));

  messages::JobDescription jd(m_originId);
  jd.insert(jp);
  m_jobDescriptionTransmitter->transmitJobDescription(
    std::move(jd), nn, finishedCB);
}

void
CNF::sendResult(NetworkedNode* nn,
                CNFTree::Path p,
                SendFinishedCB finishedCallback)
{
  assert(rootTaskReady.isReady());

  std::shared_lock lock(m_resultsMutex);

  auto resultIt = m_results.find(p);
  if(resultIt == m_results.end()) {
    PARACUBER_LOG(m_logger, LocalError)
      << "Could not find result for path " << CNFTree::pathToStrNoAlloc(p)
      << "! Cannot send result.";
    return;
  }
  auto& result = resultIt->second;

  messages::JobResult::State jobResultState = [&result]() {
    switch(result.state) {
      case CNFTree::SAT:
        return messages::JobResult::State::SAT;
      case CNFTree::UNSAT:
        return messages::JobResult::State::UNSAT;
      default:
        return messages::JobResult::State::UNKNOWN;
    }
  }();

  auto jobResult = messages::JobResult(result.p, jobResultState);

  if(jobResultState == messages::JobResult::State::SAT) {
    result.encodeAssignment();
    jobResult.initDataVec() = *result.encodedAssignment;
  }

  auto jd = messages::JobDescription(m_originId);
  jd.insert(jobResult);

  m_jobDescriptionTransmitter->transmitJobDescription(
    std::move(jd), nn, finishedCallback);
}

static CNFTree::State
jrStateToCNFTreeState(messages::JobResult::State s)
{
  switch(s) {
    case messages::JobResult::SAT:
      return CNFTree::SAT;
    case messages::JobResult::UNSAT:
      return CNFTree::UNSAT;
    case messages::JobResult::UNKNOWN:
      return CNFTree::Unknown;
    default:
      return CNFTree::Unknown;
  }
}

void
CNF::receiveJobDescription(int64_t sentFromID, messages::JobDescription&& jd)
{
  switch(jd.getKind()) {
    case messages::JobDescription::Kind::Path: {
      const auto jp = jd.getJobPath();
      std::unique_lock lock(m_cnfTreeMutex);
      auto trace = jp.getTrace();
      for(uint8_t i = 0; i < trace.size(); ++i) {
        int var = FastAbsolute(trace[i]);
        m_cnfTree->setDecision(
          CNFTree::setDepth(jp.getPath(), i), var, sentFromID);
      }
      m_cnfTree->setDecisionAndState(jp.getPath(), 0, CNFTree::Unvisited);
      m_taskFactory->addPath(
        jp.getPath(), TaskFactory::CubeOrSolve, jd.getOriginatorID());
      break;
    }
    case messages::JobDescription::Kind::Result: {
      const auto jr = jd.getJobResult();

      Result res;
      res.p = jr.getPath();
      res.state = jrStateToCNFTreeState(jr.getState());
      if(res.state == CNFTree::SAT) {
        res.size = jr.getDataVec().size();
        res.encodedAssignment =
          std::make_shared<AssignmentVector>(std::move(jr.getDataVec()));
      } else {
        res.size = 0;
      }
      res.finished = true;

      {
        std::unique_lock lock(m_resultsMutex);
        m_results.insert(std::make_pair(jr.getPath(), std::move(res)));
      }
      {
        std::unique_lock lock(m_cnfTreeMutex);
        m_cnfTree->setState(res.p, res.state, true);
      }
      break;
    }
    case messages::JobDescription::Kind::Initiator: {
      const auto ji = jd.getJobInitiator();

      if(!m_cuberRegistry)
        m_cuberRegistry =
          std::make_unique<cuber::Registry>(m_config, m_log, *this);

      switch(ji.getCubingKind()) {
        case messages::JobInitiator::PregeneratedCubes:
          m_cuberRegistry->init(cuber::Registry::PregeneratedCubes, &ji);
          break;
        case messages::JobInitiator::LiteralFrequency:
          m_cuberRegistry->init(cuber::Registry::LiteralFrequency, &ji);
          m_cuberRegistry->getAllowanceMap() = ji.getAllowanceMap();
          break;
      }

      rootTaskReady.setReady(m_rootTask.get());
      break;
    }
  }
}

void
CNF::connectToCNFTreeSignal()
{
  m_cnfTree->getRootStateChangedSignal().connect(
    [this](CNFTree::Path p, CNFTree::State state) {
      PARACUBER_LOG(m_logger, Info) << "CNF: Found a result and send to all "
                                       "subscribers of signals! End Result: "
                                    << state;

      std::shared_lock lock(m_resultsMutex);
      Result* result = &m_results[0];
      assert(result);

      if(result->state == CNFTree::SAT) {
        // The result must contain the assignment if it is satisfiable.
        result->decodeAssignment();
      }

      m_resultSignal(result);
    });
}

void
CNF::sendCB(SendDataStruct* data, boost::asio::ip::tcp::socket* socket)
{
  if(data->offset >= m_fileSize) {
    data->cb();
    // This not only erases the send data structure,
    // but also frees the last reference to the
    // shared_ptr of the TCPClient instance that was
    // calling this function. The connection is then
    // closed.
    m_sendData.erase(socket);
  } else {
    // Repeated sends, as long as the file is transferred.
    send(socket, data->cb, false);
  }
}
bool
CNF::readyToBeStarted() const
{
  return m_rootTask && m_cuberRegistry &&
         m_cuberRegistry->allowanceMapWaiter.isReady();
}

void
CNF::requestInfoGlobally(CNFTree::Path p, int64_t handle)
{
  Communicator* comm = m_config->getCommunicator();
  std::shared_lock lock(m_cnfTreeMutex);
  m_cnfTree->visit(
    p,
    [this, handle, comm, p](CNFTree::CubeVar var,
                            uint8_t depth,
                            CNFTree::State state,
                            int64_t remote) {
      if(remote == 0) {
        if(depth == CNFTree::getDepth(p)) {
          comm->injectCNFTreeNodeInfo(m_originId,
                                      handle,
                                      CNFTree::setDepth(p, depth),
                                      var,
                                      state,
                                      remote);
          return true;
        }
      } else {
        comm->sendCNFTreeNodeStatusRequest(remote, m_originId, p, handle);
        return true;
      }
      return false;
    });
}

void
CNF::solverFinishedSlot(const TaskResult& result, CNFTree::Path p)
{
  Result res;
  res.p = p;

  {
    auto& resultMut = const_cast<TaskResult&>(result);
    auto task = static_unique_pointer_cast<CaDiCaLTask>(
      std::move(resultMut.getTaskPtr()));
    res.task = std::move(task);
  }

  if(!res.task)
    return;

  res.size = res.task->getVarCount();

  switch(result.getStatus()) {
    case TaskResult::Satisfiable:
      res.state = CNFTree::SAT;

      // Satisfiable results must be directly wrapped in the result array, as
      // the solver instances are shared between solver tasks.
      PARACUBER_LOG(m_logger, Trace)
        << "Encode Assignment because SAT encountered!";
      res.encodeAssignment();
      break;
    case TaskResult::Unsatisfiable:
      res.state = CNFTree::UNSAT;
      break;
    default:
      // Other results are not handled here.
      PARACUBER_LOG(m_logger, LocalError)
        << "Invalid status received for finished solver task: "
        << result.getStatus();
  }

  // Give back the solver handle from the result after it was fully processed.
  res.task->releaseSolver();

  if(res.state == CNFTree::SAT || res.state == CNFTree::UNSAT ||
     res.state == CNFTree::Unknown) {
    {
      std::unique_lock lock(m_resultsMutex);
      m_results.insert(std::make_pair(p, std::move(res)));
    }

    if(res.state != CNFTree::Unknown) {
      std::unique_lock lock(m_cnfTreeMutex);
      m_cnfTree->setState(p, res.state);
    }
  }
}

template<typename T>
T*
receiveValueFromBuffer(CNF::ReceiveDataStruct& d, char** buf, std::size_t* len)
{
  T* returnVal = nullptr;
  if(d.receiveVarPos == 0 && *len >= sizeof(T)) {
    // Easy receive.
    returnVal = reinterpret_cast<T*>(*buf);
    *buf += sizeof(T);
    *len -= sizeof(T);
  } else {
    // Cube var split into multiple buffers, reconstructing.
    size_t l = (*len >= sizeof(T) ? sizeof(T) : *len) - d.receiveVarPos;

    std::copy(*buf, *buf + l, d.receiveVarBuf + d.receiveVarPos);

    *buf += l;
    *len -= l;

    if(d.receiveVarPos == sizeof(T)) {
      d.receiveVarPos = 0;
      returnVal = reinterpret_cast<T*>(d.receiveVarBuf);
    }
  }
  return returnVal;
}

void
CNF::receive(boost::asio::ip::tcp::socket* socket,
             const char* buf,
             std::size_t length)
{
  using namespace boost::filesystem;
  assert(socket);

  ReceiveDataStruct& d = m_receiveData[socket];

  while(length > 0 || buf == nullptr) {
    switch(d.state) {
      case ReceiveFileName: {
        // First, receive the filename.
        bool end = false;
        while(length > 0 && !end) {
          if(*buf == '\0') {
            d.state = ReceiveFile;

            path dir =
              temp_directory_path() / ("paracuber-" + std::to_string(getpid()));
            path p = dir / unique_path();
            p += "-" + m_dimacsFile;

            m_dimacsFile = p.string();

            if(!exists(dir)) {
              create_directory(dir);
            }

            m_ofstream.open(m_dimacsFile, std::ios::out);
            end = true;
          } else {
            m_dimacsFile += *buf;
          }
          ++buf;
          --length;
        }
        break;
      }
      case ReceiveFile:
        if(length == 0) {
          // This marks the end of the transmission, the file is finished.
          m_ofstream.close();
          m_receiveData.erase(socket);
          return;
        }

        m_ofstream.write(buf, length);
        length = 0;
        break;
    }
  }
}

void
CNF::setRootTask(std::unique_ptr<CaDiCaLTask> root)
{
  /// This shall only be called once, the internal root task must therefore
  /// always be false. This is because this function also initialises the cubing
  /// registry in m_cuberRegistry.
  assert(!m_rootTask);
  m_rootTask = std::move(root);

  if(!m_config->isDaemonMode()) {
    assert(!m_cuberRegistry);

    const auto& pregenCubes = m_rootTask->getPregeneratedCubes();
    messages::JobInitiator ji;
    if(pregenCubes.size() > 0) {
      size_t realised = ji.realise(pregenCubes);
      if(realised != 0) {
        PARACUBER_LOG(m_logger, LocalError)
          << "Could not parse provided cubes into binary tree format! Error at "
             "cube "
          << realised;
      }
    }

    // The cuber registry may already have been created if this is a daemon
    // node. It can then just be re-used, as the daemon cuber registry did not
    // need the root node to be created.
    m_cuberRegistry = std::make_unique<cuber::Registry>(m_config, m_log, *this);
    if(!m_cuberRegistry->init(pregenCubes.size() > 0
                                ? cuber::Registry::PregeneratedCubes
                                : cuber::Registry::LiteralFrequency,
                              &ji)) {
      PARACUBER_LOG(m_logger, Fatal) << "Could not initialise cuber registry!";
      m_cuberRegistry.reset();
      return;
    }
  }

  if(!m_config->isDaemonMode()) {
    rootTaskReady.setReady(m_rootTask.get());
  }
}
CaDiCaLTask*
CNF::getRootTask()
{
  return m_rootTask.get();
}

void
CNF::handleFinishedResultReceived(Result& result)
{
  PARACUBER_LOG(m_logger, Trace)
    << "Finished result received! State: " << result.state << " on path "
    << CNFTree::pathToStrNoAlloc(result.p);

  // Insert result into local CNF Tree. The state change should only stay local,
  // no propagate to the node that sent this change.
  std::unique_lock lock(m_cnfTreeMutex);
  m_cnfTree->setState(result.p, result.state, true);
}

void
CNF::insertResult(CNFTree::Path p, CNFTree::State state, CNFTree::Path source)
{
  Result res = { 0 };
  res.p = p;
  res.state = state;

  PARACUBER_LOG(m_logger, Trace)
    << "Insert result " << state << " for path " << CNFTree::pathToStrNoAlloc(p)
    << " from source path " << CNFTree::pathToStrNoAlloc(source);

  // Assign the path directly to 0 to make referencing it easier, if this is the
  // root path.
  if(CNFTree::getDepth(p) == 0)
    p = 0;

  if(source != CNFTree::DefaultUninitiatedPath) {
    std::unique_lock shared_lock(m_resultsMutex);
    // Reference old result.
    auto resultIt = m_results.find(source);
    if(resultIt == m_results.end()) {
      PARACUBER_LOG(m_logger, LocalError)
        << "Could not find result reference of " << CNFTree::pathToStrNoAlloc(p)
        << " to " << CNFTree::pathToStrNoAlloc(source)
        << "! Not inserting result.";
      return;
    }
    const Result& oldResult = resultIt->second;
    res.task = oldResult.task;
    res.size = oldResult.size;
    res.encodedAssignment = oldResult.encodedAssignment;
    res.decodedAssignment = oldResult.decodedAssignment;
  }

  std::unique_lock unique_lock(m_resultsMutex);
  m_results.insert(std::make_pair(p, std::move(res)));
}

void
CNF::Result::encodeAssignment()
{
  if(encodedAssignment)
    return;
  assert(decodedAssignment || task);
  encodedAssignment = std::make_shared<AssignmentVector>();

  if(decodedAssignment) {
    // Encode from currently decoded assignment.
    SerializeAssignmentFromArray(*encodedAssignment, size, *decodedAssignment);
  } else {
    // Write directly from solver.
    task->writeEncodedAssignment(*encodedAssignment);
  }
}

void
CNF::Result::decodeAssignment()
{
  if(decodedAssignment)
    return;
  assert(encodedAssignment || task);
  decodedAssignment = std::make_shared<AssignmentVector>();

  if(encodedAssignment) {
    // Use the already encoded assignment to generate decoded one.
    DeSerializeToAssignment(*decodedAssignment, *encodedAssignment, size);
  } else {
    // Receive assignment directly from solver.
    task->writeDecodedAssignment(*decodedAssignment);
  }
}

std::ostream&
operator<<(std::ostream& o, CNF::ReceiveState s)
{
  switch(s) {
    case CNF::ReceiveState::ReceiveFile:
      o << "Receive File";
      break;
    case CNF::ReceiveState::ReceiveFileName:
      o << "Receive File Name";
      break;
    default:
      o << "(! UNKNOWN RECEIVE STATE !)";
      break;
  }
  return o;
}

std::ostream&
operator<<(std::ostream& o, CNF::CubingKind k)
{
  switch(k) {
    case CNF::CubingKind::LiteralFrequency:
      o << "LiteralFrequency";
      break;
    case CNF::CubingKind::PregeneratedCubes:
      o << "PregeneratedCubes";
      break;
    default:
      o << "(! UNKNOWN CUBING KIND !)";
      break;
  }
  return o;
}
}
