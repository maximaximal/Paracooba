#include "broker_compute_node.hpp"
#include "broker_compute_node_store.hpp"
#include "broker_task_store.hpp"
#include "cereal/details/helpers.hpp"
#include "paracooba/common/compute_node_store.h"
#include "paracooba/common/file.h"
#include "paracooba/common/message_kind.h"
#include "paracooba/common/status.h"
#include "paracooba/common/task.h"
#include "paracooba/common/task_store.h"
#include "paracooba/communicator/communicator.h"
#include "paracooba/module.h"
#include "paracooba/solver/solver.h"

#include <cmath>
#include <paracooba/common/compute_node.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/noncopy_ostream.hpp>

#include "../commonc/spin_lock.hpp"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <numeric>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/numeric.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

using boost::accumulate;
using boost::adaptors::map_values;
using boost::adaptors::transformed;
using std::bind;
using std::mem_fn;

namespace parac::broker {
struct MessageKnownRemotes {
  struct Remote {
    Remote() = default;
    Remote(parac_id id, const std::string& s)
      : id(id)
      , connectionString(s) {}
    Remote(std::pair<parac_id, std::string> p)
      : Remote(p.first, p.second) {}
    parac_id id;
    std::string connectionString;

    template<class Archive>
    void serialize(Archive& ar) {
      ar(CEREAL_NVP(id), CEREAL_NVP(connectionString));
    }
  };
  using Vec = std::vector<Remote>;
  Vec remotes;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(remotes));
  }

  Vec::iterator begin() { return remotes.begin(); }
  Vec::iterator end() { return remotes.end(); }
  Vec::const_iterator begin() const { return remotes.begin(); }
  Vec::const_iterator end() const { return remotes.end(); }
};

ComputeNode::ComputeNode(parac_compute_node& node,
                         parac_handle& handle,
                         ComputeNodeStore& store,
                         TaskStore& taskStore)
  : m_node(node)
  , m_handle(handle)
  , m_store(store)
  , m_taskStore(taskStore) {
  node.receive_message_from = [](parac_compute_node* n, parac_message* msg) {
    assert(n);
    assert(n->broker_userdata);
    assert(msg);
    ComputeNode* self = static_cast<ComputeNode*>(n->broker_userdata);
    self->receiveMessageFrom(*msg);
  };
  node.receive_file_from = [](parac_compute_node* n, parac_file* file) {
    assert(n);
    assert(n->broker_userdata);
    assert(file);
    ComputeNode* self = static_cast<ComputeNode*>(n->broker_userdata);
    self->receiveFileFrom(*file);
  };
}
ComputeNode::~ComputeNode() {}

ComputeNode::Description::Description() {}
ComputeNode::Description::Description(std::string name,
                                      std::string host,
                                      uint32_t workers,
                                      uint16_t udpListenPort,
                                      uint16_t tcpListenPort,
                                      bool daemon,
                                      bool local)
  : name(name)
  , host(host)
  , workers(workers)
  , udpListenPort(udpListenPort)
  , tcpListenPort(tcpListenPort)
  , daemon(daemon)
  , local(local) {}

void
ComputeNode::Description::serializeToMessage(parac_message& msg) const {
  msg.kind = PARAC_MESSAGE_NODE_DESCRIPTION;

  if(!m_descriptionStream) {
    m_descriptionStream = std::make_unique<NoncopyOStringstream>();
  } else {
    *m_descriptionStream = NoncopyOStringstream();
  }

  assert(m_descriptionStream->tellp() == 0);

  {
    cereal::BinaryOutputArchive oa(*m_descriptionStream);
    oa(*this);
  }

  msg.data = m_descriptionStream->ptr();
  msg.length = m_descriptionStream->tellp();
}

uint64_t
ComputeNode::Status::workQueueSize() const {
  if(m_dirty) {
    m_cachedWorkQueueSize =
      accumulate(solverInstances | map_values |
                   transformed(mem_fn(&SolverInstance::workQueueSize)),
                 0);
    resetDirty();
  }
  return m_cachedWorkQueueSize;
}

void
ComputeNode::Status::serializeToMessage(parac_message& msg) const {
  msg.kind = PARAC_MESSAGE_NODE_STATUS;

  assert(checkStreamRefs());

  if(!m_statusStream) {
    m_statusStream = std::make_unique<NoncopyOStringstream>();
  } else {
    if(dirty()) {
      *m_statusStream = NoncopyOStringstream();
    }
  }

  if(dirty() || m_statusStream->tellp() == 0) {
    assert(m_statusStream->tellp() == 0);

    {
      SpinLock lock(m_writeFlag);
      cereal::BinaryOutputArchive oa(*m_statusStream);
      oa(*this);
    }

    workQueueSize();
  }

  msg.data = m_statusStream->ptr();
  msg.length = m_statusStream->tellp();
}
bool
ComputeNode::Status::isParsed(parac_id id) const {
  auto it = solverInstances.find(id);
  if(it == solverInstances.end())
    return false;
  return it->second.formula_parsed;
}

void
ComputeNode::incrementWorkQueueSize(parac_id originator) {
  SpinLock lock(m_status.m_writeFlag);
  ++m_status.solverInstances[originator].workQueueSize;
  m_status.m_dirty = true;
}
void
ComputeNode::decrementWorkQueueSize(parac_id originator) {
  SpinLock lock(m_status.m_writeFlag);
  --m_status.solverInstances[originator].workQueueSize;
  m_status.m_dirty = true;
}
void
ComputeNode::formulaParsed(parac_id originator) {
  SpinLock lock(m_status.m_writeFlag);
  m_status.solverInstances[originator].formula_parsed = true;
  m_status.m_dirty = true;
}

void
ComputeNode::initDescription(const std::string& name,
                             const std::string& host,
                             uint32_t workers,
                             uint16_t udpListenPort,
                             uint16_t tcpListenPort,
                             bool demon,
                             bool local) {
  m_description = Description(
    name, host, workers, udpListenPort, tcpListenPort, demon, local);
}

void
ComputeNode::applyStatus(const Status& s) {
  m_status.solverInstances = s.solverInstances;
}

void
ComputeNode::receiveMessageFrom(parac_message& msg) {
  if(parac_message_kind_is_for_solver(msg.kind)) {
    ComputeNode* n = m_store.get_broker_compute_node(msg.originator_id);
    assert(n);
    parac_module_solver_instance* i = n->getSolverInstance();
    i->handle_message(i, &msg);
    return;
  }

  try {
    switch(msg.kind) {
      case PARAC_MESSAGE_NODE_DESCRIPTION:
        receiveMessageDescriptionFrom(msg);
        break;
      case PARAC_MESSAGE_NODE_STATUS:
        receiveMessageStatusFrom(msg);
        break;
      case PARAC_MESSAGE_TASK_RESULT:
        receiveMessageTaskResultFrom(msg);
        break;
      case PARAC_MESSAGE_NEW_REMOTES:
        receiveMessageKnownRemotesFrom(msg);
        break;
      default:
        parac_log(PARAC_BROKER,
                  PARAC_GLOBALERROR,
                  "Invalid message kind received in compute node! Kind: {}, "
                  "size: {}, origin: {}, originator: {}",
                  msg.kind,
                  msg.length,
                  m_node.id,
                  msg.originator_id);
        msg.cb(&msg, PARAC_UNKNOWN);
        break;
    }
  } catch(cereal::Exception& e) {
    parac_log(PARAC_BROKER,
              PARAC_GLOBALERROR,
              "Could not parse message received in compute node! Kind: {}, "
              "size: {}, origin: {}, originator: {}, error: {}",
              msg.kind,
              msg.length,
              m_node.id,
              msg.originator_id,
              e.what());
    msg.cb(&msg, PARAC_ABORT_CONNECTION);
  } catch(std::exception& e) {
    parac_log(PARAC_BROKER,
              PARAC_LOCALERROR,
              "Exception when processing message of kind: {}, "
              "size: {}, origin: {}, originator: {}. Exception: {}",
              msg.kind,
              msg.length,
              m_node.id,
              msg.originator_id,
              e.what());
  } catch(...) {
    assert(false);
  }
}

void
ComputeNode::receiveMessageDescriptionFrom(parac_message& msg) {
  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg.data, msg.length);

  assert(msg.origin->id == m_node.id);

  m_description = Description();
  {
    cereal::BinaryInputArchive ia(data);
    ia(*m_description);
  }
  msg.cb(&msg, PARAC_OK);

  parac_log(PARAC_BROKER,
            PARAC_TRACE,
            "Received description from node ID {}! Description: {}.",
            msg.origin->id,
            *m_description);

  if(!m_description->daemon && !m_node.solver_instance) {
    auto solverMod = m_handle.modules[PARAC_MOD_SOLVER];
    assert(solverMod);
    auto solver = solverMod->solver;
    assert(solver);
    m_node.solver_instance =
      solver->add_instance(solverMod, m_node.id, &m_taskStore.store());
  }

  // Send all known other nodes to the peer.
  sendKnownRemotes();

  if(m_handle.input_file &&
     !m_status.solverInstances[m_handle.id].formula_received) {
    // Client node received a message from other node.
    // =================================================================

    // First, solver configuration - then send file to parse.
    parac_module_solver_instance* instance =
      m_store.thisNode().m_node.solver_instance;
    assert(instance);

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Send solver config to node ID {}!",
              m_handle.id);

    parac_message_wrapper msg;
    instance->serialize_config_to_msg(instance, &msg);

    msg.userdata = this;
    msg.cb = [](parac_message* msg, parac_status s) {
      if(s == PARAC_OK) {
        assert(msg->userdata);
        auto self = static_cast<ComputeNode*>(msg->userdata);

        parac_module_solver_instance* instance =
          self->m_store.thisNode().m_node.solver_instance;
        assert(instance);

        parac_log(PARAC_BROKER,
                  PARAC_TRACE,
                  "Send formula in file {} with originator {} to node ID {}!",
                  self->m_handle.input_file,
                  instance->originator_id,
                  self->m_handle.id);

        // This is a client node with an input file! Send the input file to the
        // peer. Once the formula was parsed, a status is sent.
        parac_file_wrapper formula;
        formula.path = self->m_handle.input_file;
        formula.originator = self->m_handle.id;
        self->m_node.send_file_to(&self->m_node, &formula);
      }
    };

    m_node.send_message_to(&m_node, &msg);
  }
}

void
ComputeNode::receiveMessageStatusFrom(parac_message& msg) {
  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg.data, msg.length);

  {
    cereal::BinaryInputArchive ia(data);
    ia(m_status);

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Got status from {}: {}, message length: {}B",
              msg.origin->id,
              m_status,
              msg.length);
  }

  // Check if the target needs some tasks according to offloading heuristic.
  float utilization = computeUtilization();
  if(utilization < 3) {
    // Tasks need to be offloaded!
    tryToOffloadTask();
  }

  msg.cb(&msg, PARAC_OK);
}
void
ComputeNode::receiveMessageTaskResultFrom(parac_message& msg) {
  m_taskStore.receiveTaskResultFromPeer(msg);
}

bool
ComputeNode::tryToOffloadTask() {
  assert(description());
  if(description()->workers == 0) {
    return false;
  }

  parac_task* task = m_taskStore.pop_offload(
    &m_node, [this](parac_task& t) { return status().isParsed(t.originator); });

  if(task) {
    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "Offload task on path {} to node {}.",
              task->path,
              m_node.id);

    assert(status().isParsed(task->originator));

    parac_message_wrapper msg;
    task->serialize(task, &msg);
    msg.userdata = task;
    msg.cb = [](parac_message* msg, parac_status s) {
      if(s != PARAC_OK && s != PARAC_TO_BE_DELETED) {
        // Message has been lost! Undo offload operation.
        parac_task* t = static_cast<parac_task*>(msg->userdata);
        parac_log(PARAC_BROKER,
                  PARAC_LOCALERROR,
                  "Offload operation of task on path {} to remote node {} "
                  "failed! Undoing offload.",
                  t->path,
                  t->offloaded_to->id);
        t->task_store->undo_offload(t->task_store, t);
      }
    };
    m_node.send_message_to(&m_node, &msg);
    return true;
  }
  return false;
}

void
ComputeNode::receiveMessageKnownRemotesFrom(parac_message& msg) {
  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg.data, msg.length);

  MessageKnownRemotes remotes;

  {
    cereal::BinaryInputArchive ia(data);
    ia(remotes);
  }

  for(const auto& r : remotes) {
    if(m_store.has(r.id)) {
      continue;
    }

    parac_log(PARAC_BROKER,
              PARAC_TRACE,
              "New Known Remote from {}: {} @ {}. Trying to connect.",
              m_node.id,
              r.id,
              r.connectionString);

    assert(m_handle.modules);
    auto mod_comm = m_handle.modules[PARAC_MOD_COMMUNICATOR];
    auto comm = mod_comm->communicator;
    comm->connect_to_remote(mod_comm, r.connectionString.c_str());
  }

  msg.cb(&msg, PARAC_OK);
}

void
ComputeNode::receiveFileFrom(parac_file& file) {
  if(!m_handle.input_file) {
    auto& solverInstance = m_status.solverInstances[file.originator];
    if(solverInstance.formula_received) {
      parac_log(PARAC_BROKER,
                PARAC_GLOBALERROR,
                "Formula with originator {} received more than once!",
                file.originator);

      file.cb(&file, PARAC_FORMULA_RECEIVED_TWICE_ERROR);
    } else {
      // The received file must be the formula! It should be parsed and then
      // waited on for tasks.
      assert(m_node.solver_instance);
      assert(m_node.solver_instance->handle_formula);
      m_node.solver_instance->handle_formula(
        m_node.solver_instance,
        &file,
        this,
        [](parac_module_solver_instance* instance,
           void* userdata,
           parac_status status) {
          (void)instance;

          ComputeNode* self = static_cast<ComputeNode*>(userdata);

          if(status == PARAC_OK) {
            self->m_store.formulaParsed(self->m_node.id);
          }
        });
    }
  }
}

float
ComputeNode::computeUtilization() const {
  // Utilization cannot be computed, so it is assumed to be infinite.
  if(!description())
    return FP_INFINITE;

  float workers = description()->workers;
  float workQueueSize = status().workQueueSize();

  if(workers == 0) {
    return FP_INFINITE;
  }

  return workQueueSize / workers;
}

parac_module_solver_instance*
ComputeNode::getSolverInstance() {
  return m_node.solver_instance;
}

void
ComputeNode::sendKnownRemotes() {
  if(!m_knownRemotesOstream) {
    m_knownRemotesOstream = std::make_unique<NoncopyOStringstream>();
  } else {
    *m_knownRemotesOstream = NoncopyOStringstream();
  }

  parac_message_wrapper msg;
  msg.kind = PARAC_MESSAGE_NEW_REMOTES;

  MessageKnownRemotes knownRemotes;

  auto stepOverNode = [this](const parac_compute_node_wrapper& node) {
    return node.id == m_node.id || !node.send_message_to ||
           !node.connection_string;
  };
  for(const auto& node : m_store) {
    if(stepOverNode(node))
      continue;
    knownRemotes.remotes.emplace_back(node.getIDConnectionStringPair());
  }

  {
    cereal::BinaryOutputArchive oa(*m_knownRemotesOstream);
    oa(knownRemotes);
  }

  msg.data = m_knownRemotesOstream->ptr();
  msg.length = m_knownRemotesOstream->tellp();

  m_node.send_message_to(&m_node, &msg);
}

std::ostream&
operator<<(std::ostream& o, const ComputeNode::Description& d) {
  return o << d.name << "@" << d.host << " " << (d.daemon ? "daemon" : "client")
           << " with " << d.workers << " available workers";
}
std::ostream&
operator<<(std::ostream& o, const ComputeNode::SolverInstance& si) {
  return o << (si.formula_parsed ? "parsed" : "unparsed") << ", "
           << si.workQueueSize << " tasks";
}
std::ostream&
operator<<(std::ostream& o, const ComputeNode::Status& s) {
  o << "Work queue size: " << s.workQueueSize() << ", containing "
    << s.solverInstances.size() << " instances:";
  for(auto& i : s.solverInstances) {
    o << " { " << i.first << ":" << i.second << " }";
  }
  return o;
}
}
