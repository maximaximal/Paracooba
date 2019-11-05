#include "../include/paracuber/cnf.hpp"
#include "../include/paracuber/cadical_task.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/cuber/registry.hpp"
#include "../include/paracuber/runner.hpp"
#include "../include/paracuber/task_factory.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <iostream>

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
  , m_logger(log->createLogger())
  , m_cnfTree(std::make_unique<CNFTree>(config, originId))
{
  if(dimacsFile != "") {
    struct stat statbuf;
    int result = stat(m_dimacsFile.c_str(), &statbuf);
    assert(result != -1);

    m_fileSize = statbuf.st_size;
    m_fd = open(m_dimacsFile.c_str(), O_RDONLY);
    assert(m_fd > 0);
  }
}
CNF::CNF(const CNF& o)
  : m_config(o.m_config)
  , m_originId(o.m_originId)
  , m_dimacsFile(o.m_dimacsFile)
  , m_log(o.m_log)
  , m_logger(o.m_log->createLogger())
  , m_cnfTree(std::make_unique<CNFTree>(o.m_config, o.m_originId))
{}

CNF::~CNF() {}

void
CNF::send(boost::asio::ip::tcp::socket* socket,
          CNFTree::Path path,
          SendFinishedCB cb,
          bool first)
{
  SendDataStruct* data = &m_sendData[socket];

  if(first) {
    data->cb = cb;
    data->offset = 0;

    // Write the subject.
    TransmissionSubject subject = TransmitFormula;
    socket->write_some(boost::asio::buffer(
      reinterpret_cast<const char*>(&subject), sizeof(subject)));

    // Write the originator ID.
    int64_t id = m_config->getInt64(Config::Id);
    socket->write_some(
      boost::asio::buffer(reinterpret_cast<const char*>(&id), sizeof(id)));

    // Send the path to use.
    socket->write_some(
      boost::asio::buffer(reinterpret_cast<const char*>(&path), sizeof(path)));
  }

  if(path == 0) {
    if(first) {
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
    int ret =
      sendfile(socket->native_handle(), m_fd, &data->offset, m_fileSize);
    if(ret == -1) {
      std::cerr << "ERROR DURING SENDFILE: " << strerror(errno) << std::endl;
      m_sendData.erase(socket);
    }

    socket->async_write_some(boost::asio::null_buffers(),
                             std::bind(&CNF::sendCB, this, data, path, socket));
  } else {
    first = false;

    // Transmit all decisions on the given path.
    data->decisions.reserve(CNFTree::getDepth(path) - 1);
    m_cnfTree->writePathToLiteralContainer(
      data->decisions, CNFTree::setDepth(path, CNFTree::getDepth(path) - 1));

    // After this async write operation has finished, the local data can be
    // erased again. This happens when the file offset reaches the file size, so
    // the file offset is set to the filesize in this statement.
    data->offset = m_fileSize;

    socket->async_write_some(
      boost::asio::buffer(reinterpret_cast<const char*>(data->decisions.data()),
                          data->decisions.size() * sizeof(CNFTree::CubeVar)),
      std::bind(&CNF::sendCB, this, data, path, socket));
  }
}

void
CNF::sendAllowanceMap(boost::asio::ip::tcp::socket* socket,
                      SendFinishedCB finishedCallback)
{
  assert(rootTaskReady.isReady());
  assert(m_cuberRegistry->allowanceMapWaiter.isReady());

  auto& map = m_cuberRegistry->getAllowanceMap();

  // The allowance map may take a while to generate.
  assert(map.size() > 0);

  // Write the subject (an AllowanceMap).
  TransmissionSubject subject = TransmitAllowanceMap;
  socket->write_some(boost::asio::buffer(
    reinterpret_cast<const char*>(&subject), sizeof(subject)));

  // Write the originator ID.
  int64_t id = m_config->getInt64(Config::Id);
  socket->write_some(
    boost::asio::buffer(reinterpret_cast<const char*>(&id), sizeof(id)));

  // Write the map size.
  uint32_t mapSize = map.size();
  socket->write_some(boost::asio::buffer(
    reinterpret_cast<const char*>(&mapSize), sizeof(mapSize)));

  // Write the map itself asynchronously.
  socket->async_write_some(
    boost::asio::buffer(reinterpret_cast<const char*>(map.data()),
                        mapSize * sizeof(map[0])),
    std::bind(finishedCallback));
}

void
CNF::sendResult(boost::asio::ip::tcp::socket* socket,
                CNFTree::Path p,
                SendFinishedCB finishedCallback)
{
  assert(rootTaskReady.isReady());

  auto resultIt = m_results.find(p);
  if(resultIt == m_results.end()) {
    // No result in result map! Therefore, the solver state must be saved in the
    // CNFTree.
    CNFTree::State s;
    CNFTree::Path parentPath = CNFTree::getParent(p);
    m_cnfTree->getState(parentPath, s);

    // The state must either be SAT or UNSAT for this to be normal control flow.
    switch(s) {
      case CNFTree::SAT:
        break;
      case CNFTree::UNSAT:
        break;
      default:
        PARACUBER_LOG(m_logger, LocalWarning)
          << "No result found for path " << CNFTree::pathToStrNoAlloc(p)
          << " and the state in the CNFTree for parent is " << s << "!";
        return;
    }

    Result res;
    res.state = s;

    resultIt = m_results.insert(std::make_pair(p, std::move(res))).first;
  }
  const auto& result = resultIt->second;

  // Write the subject (a Result).
  TransmissionSubject subject = TransmitResult;
  socket->write_some(boost::asio::buffer(
    reinterpret_cast<const char*>(&subject), sizeof(subject)));

  // Write the originator ID.
  int64_t id = m_config->getInt64(Config::Id);
  socket->write_some(
    boost::asio::buffer(reinterpret_cast<const char*>(&id), sizeof(id)));

  // Write the path.
  socket->write_some(
    boost::asio::buffer(reinterpret_cast<const char*>(&p), sizeof(p)));

  // Write the result state from the CNFTree.
  CNFTree::State state;
  m_cnfTree->getState(p, state);
  socket->write_some(
    boost::asio::buffer(reinterpret_cast<const char*>(&state), sizeof(state)));

  // Write the result size.
  uint32_t resultSize = result.assignment.size();
  socket->write_some(boost::asio::buffer(
    reinterpret_cast<const char*>(&resultSize), sizeof(resultSize)));

  if(state == CNFTree::SAT) {
    // Write the full result.
    socket->async_write_some(
      boost::asio::buffer(
        reinterpret_cast<const char*>(result.assignment.data()),
        result.size * sizeof(result.assignment[0])),
      std::bind(finishedCallback));
  } else {
    // Write the UNSAT proof.
    finishedCallback();
  }
}

void
CNF::sendCB(SendDataStruct* data,
            CNFTree::Path path,
            boost::asio::ip::tcp::socket* socket)
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
    send(socket, path, data->cb, false);
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

  CNFTree::State state = CNFTree::Unknown;

  {
    auto& resultMut = const_cast<TaskResult&>(result);
    auto task = static_unique_pointer_cast<CaDiCaLTask>(
      std::move(resultMut.getTaskPtr()));
    res.task = std::move(task);
  }

  res.size = res.task->getVarCount();

  switch(result.getStatus()) {
    case TaskResult::Satisfiable:
      state = CNFTree::SAT;
      // The result assignment must be received from the solver too and written
      // into an uint8_t vector for efficient transfer.
      //
      // TODO!!
      break;
    case TaskResult::Unsatisfiable:
      state = CNFTree::UNSAT;
      break;
    default:
      // Other results are not handled here.
      PARACUBER_LOG(m_logger, LocalError)
        << "Invalid status received for finished solver task: "
        << result.getStatus();
      return;
  }

  m_results.insert(std::make_pair(p, std::move(res)));

  if(state != CNFTree::Unknown) {
    m_cnfTree->setState(p, state);
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

#define ERASE_AND_RETURN_SUBJECT()        \
  {                                       \
    TransmissionSubject subj = d.subject; \
    m_receiveData.erase(socket);          \
    return subj;                          \
  }

CNF::TransmissionSubject
CNF::receive(boost::asio::ip::tcp::socket* socket,
             char* buf,
             std::size_t length)
{
  using namespace boost::filesystem;
  assert(socket);

  ReceiveDataStruct& d = m_receiveData[socket];

  while(length > 0 || buf == nullptr) {
    switch(d.state) {
      case ReceiveTransmissionSubject:
        if(length < (sizeof(TransmissionSubject) + sizeof(int64_t))) {
          // Remote has directly aborted the transmission.
          PARACUBER_LOG(m_logger, GlobalError)
            << "Remote "
            << ((socket != nullptr)
                  ? socket->remote_endpoint().address().to_string()
                  : "(Unknwon)")
            << " aborted TCP transmission before transmitting subject.";
          ERASE_AND_RETURN_SUBJECT()
        }
        d.subject = *reinterpret_cast<TransmissionSubject*>(buf);
        buf += sizeof(TransmissionSubject);
        length -= sizeof(TransmissionSubject);

        d.originator = *reinterpret_cast<int64_t*>(buf);
        buf += sizeof(int64_t);
        length -= sizeof(int64_t);

        switch(d.subject) {
          case TransmitFormula:
            d.state = ReceivePath;
            break;
          case TransmitAllowanceMap:
            d.state = ReceiveAllowanceMapSize;
            break;
          case TransmitResult:
            d.state = ReceiveResultPath;
            break;
          default:
            PARACUBER_LOG(m_logger, GlobalError)
              << "Unknown subject received from " << socket->remote_endpoint()
              << "!";
            ERASE_AND_RETURN_SUBJECT()
        }

        PARACUBER_LOG(m_logger, Trace)
          << "Receive TCP transmission (" << d.subject << ") from "
          << socket->remote_endpoint() << " (ID: " << d.originator << ")";

        break;
      case ReceivePath:
        if(length < sizeof(ReceiveDataStruct::path)) {
          // Remote has directly aborted the transmission.
          ERASE_AND_RETURN_SUBJECT()
        }
        d.path = *(reinterpret_cast<int64_t*>(buf));
        buf += sizeof(ReceiveDataStruct::path);
        length -= sizeof(ReceiveDataStruct::path);

        if(d.path == 0) {
          d.state = ReceiveFileName;
        } else {
          d.state = ReceiveCube;
        }
        break;
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
          ERASE_AND_RETURN_SUBJECT()
        }

        m_ofstream.write(buf, length);
        length = 0;
        break;
      case ReceiveCube:
        if(length == 0 && buf == nullptr) {
          // This marks the end of the transmission, the decision sequence is
          // finished.

          // This also means, the task factory needs to get this newly received
          // path.
          assert(m_taskFactory);
          m_cnfTree->setDecisionAndState(d.path, 0, CNFTree::Unvisited);
          m_taskFactory->addPath(
            d.path, TaskFactory::CubeOrSolve, d.originator);

          ERASE_AND_RETURN_SUBJECT()
        }
        for(std::size_t i = 0; i < length; i += sizeof(CNFTree::CubeVar)) {
          CNFTree::CubeVar* var =
            receiveValueFromBuffer<CNFTree::CubeVar>(d, &buf, &length);
          if(var) {
            *var = FastAbsolute(*var);
            // Valid literal received, insert into CNFTree.
            if(!m_cnfTree->setDecision(
                 CNFTree::setDepth(d.path, d.currentDepth++),
                 *var,
                 d.originator)) {
              PARACUBER_LOG(m_logger, LocalError)
                << "Could not apply decision <" << *var
                << "> into CNFTree of CNF " << m_originId;
            }
          }
        }
        break;
      case ReceiveAllowanceMapSize: {
        if(!m_cuberRegistry) {
          m_cuberRegistry =
            std::make_unique<cuber::Registry>(m_config, m_log, *this);
          if(!m_cuberRegistry->init()) {
            PARACUBER_LOG(m_logger, Fatal)
              << "Could not initialise cuber registry!";
            m_cuberRegistry.reset();
            return d.subject;
          }
        }
        uint32_t* size = receiveValueFromBuffer<uint32_t>(d, &buf, &length);
        if(size) {
          PARACUBER_LOG(m_logger, Trace)
            << "Receive allowance map from " << socket->remote_endpoint()
            << " with size " << *size << ".";
          m_cuberRegistry->getAllowanceMap().clear();
          m_cuberRegistry->getAllowanceMap().reserve(*size);
        }
        d.state = ReceiveAllowanceMap;
        break;
      }
      case ReceiveAllowanceMap: {
        if(length == 0 && buf == nullptr) {
          // This marks the end of the transmission, the decision sequence is
          // finished.
          m_cuberRegistry->allowanceMapWaiter.setReady(
            &m_cuberRegistry->getAllowanceMap());
          ERASE_AND_RETURN_SUBJECT()
        }

        CNFTree::CubeVar* cubeVar =
          receiveValueFromBuffer<CNFTree::CubeVar>(d, &buf, &length);
        while(cubeVar != nullptr) {
          m_cuberRegistry->getAllowanceMap().push_back(*cubeVar);
          cubeVar = receiveValueFromBuffer<CNFTree::CubeVar>(d, &buf, &length);
        }

        break;
      }
      case ReceiveResultPath: {
        CNFTree::Path* path =
          receiveValueFromBuffer<CNFTree::Path>(d, &buf, &length);
        if(path) {
          d.path = *path;
          assert(m_results.find(*path) == m_results.end());
          Result r{ *path, CNFTree::State::Unknown };
          d.result = &r;
          m_results.insert(std::make_pair(*path, std::move(r)));
          d.state = ReceiveResultState;
        }
      }
      case ReceiveResultState: {
        CNFTree::State* state =
          receiveValueFromBuffer<CNFTree::State>(d, &buf, &length);
        if(state) {
          assert(d.result);
          d.result->state = *state;
          d.state = ReceiveResultSize;
        }
        break;
      }
      case ReceiveResultSize: {
        uint32_t* size = receiveValueFromBuffer<uint32_t>(d, &buf, &length);
        if(size) {
          assert(d.result);
          d.result->assignment.reserve(*size);
          d.result->size = *size;
          d.state = ReceiveResultData;
        }
        break;
      }
      case ReceiveResultData: {
        if(length == 0 && buf == nullptr) {
          // This marks the end of the transmission, the result sequence is
          // finished.
          d.result->finished = true;
          ERASE_AND_RETURN_SUBJECT()
        }

        uint8_t* next8Vals = receiveValueFromBuffer<uint8_t>(d, &buf, &length);
        while(next8Vals != nullptr) {
          assert(d.result);
          Result* r = d.result;
          uint8_t next = *next8Vals;
          // Get next assignments, depending on remaining size. At the very end,
          // only valid variables shall be extracted.
          switch(r->size - r->assignment.size()) {
            default:
              r->assignment.push_back(next & 0b00000001u);
            case 7:
              r->assignment.push_back(next & 0b00000010u);
            case 6:
              r->assignment.push_back(next & 0b00000100u);
            case 5:
              r->assignment.push_back(next & 0b00001000u);
            case 4:
              r->assignment.push_back(next & 0b00010000u);
            case 3:
              r->assignment.push_back(next & 0b00100000u);
            case 2:
              r->assignment.push_back(next & 0b01000000u);
            case 1:
              r->assignment.push_back(next & 0b10000000u);
            case 0:
              break;
          }
        }

        break;
      }
    }
  }
  return d.subject;
}

void
CNF::setRootTask(std::unique_ptr<CaDiCaLTask> root)
{
  /// This shall only be called once, the internal root task must therefore
  /// always be false. This is because this function also initialises the cubing
  /// registry in m_cuberRegistry.
  assert(!m_rootTask);
  m_rootTask = std::move(root);
  if(!m_cuberRegistry) {
    // The cuber registry may already have been created if this is a daemon
    // node. It can then just be re-used, as the daemon cuber registry did not
    // need the root node to be created.
    m_cuberRegistry = std::make_unique<cuber::Registry>(m_config, m_log, *this);
    if(!m_cuberRegistry->init()) {
      PARACUBER_LOG(m_logger, Fatal) << "Could not initialise cuber registry!";
      m_cuberRegistry.reset();
      return;
    }
  }
  rootTaskReady.setReady(m_rootTask.get());
}
CaDiCaLTask*
CNF::getRootTask()
{
  return m_rootTask.get();
}

std::ostream&
operator<<(std::ostream& o, CNF::TransmissionSubject s)
{
  switch(s) {
    case CNF::TransmissionSubject::TransmitFormula:
      o << "Transmit Formula";
      break;
    case CNF::TransmissionSubject::TransmitAllowanceMap:
      o << "Transmit Allowance Map";
      break;
    case CNF::TransmissionSubject::TransmitResult:
      o << "Transmit Result";
      break;
    default:
      o << "(! UNKNOWN TRANSMISSION SUBJECT !)";
      break;
  }
  return o;
}
std::ostream&
operator<<(std::ostream& o, CNF::ReceiveState s)
{
  switch(s) {
    case CNF::ReceiveState::ReceiveCube:
      o << "Receive Cube";
      break;
    case CNF::ReceiveState::ReceiveFile:
      o << "Receive File";
      break;
    case CNF::ReceiveState::ReceivePath:
      o << "Receive Path";
      break;
    case CNF::ReceiveState::ReceiveFileName:
      o << "Receive File Name";
      break;
    case CNF::ReceiveState::ReceiveAllowanceMap:
      o << "Receive Allowance Map";
      break;
    case CNF::ReceiveState::ReceiveAllowanceMapSize:
      o << "Receive Allowance Map Size";
      break;
    case CNF::ReceiveState::ReceiveTransmissionSubject:
      o << "Receive Transmission Subject";
      break;
    case CNF::ReceiveState::ReceiveResultData:
      o << "Receive Result Data";
      break;
    case CNF::ReceiveState::ReceiveResultPath:
      o << "Receive Result Path";
      break;
    case CNF::ReceiveState::ReceiveResultSize:
      o << "Receive Result Size";
      break;
    case CNF::ReceiveState::ReceiveResultState:
      o << "Receive Result State";
      break;
    default:
      o << "(! UNKNOWN RECEIVE STATE !)";
      break;
  }
  return o;
}
}
