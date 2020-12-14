#include "broker_compute_node.hpp"
#include "paracooba/common/message_kind.h"
#include "paracooba/module.h"
#include "paracooba/solver/solver.h"

#include <paracooba/common/compute_node.h>
#include <paracooba/common/log.h>
#include <paracooba/common/message.h>
#include <paracooba/common/noncopy_ostream.hpp>

#include <algorithm>
#include <initializer_list>
#include <numeric>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/numeric.hpp>

#include <cereal/archives/binary.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

using boost::accumulate;
using boost::adaptors::map_values;
using boost::adaptors::transformed;
using std::bind;
using std::mem_fn;

namespace parac::broker {
ComputeNode::ComputeNode(parac_compute_node& node, parac_handle& handle)
  : m_node(node)
  , m_handle(handle) {
  node.receive_message_from = [](parac_compute_node* n, parac_message* msg) {
    assert(n);
    assert(n->broker_userdata);
    assert(msg);
    ComputeNode* self = static_cast<ComputeNode*>(n->broker_userdata);
    self->receiveMessageFrom(*msg);
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
    m_descriptionStream->clear();
  }

  {
    cereal::BinaryOutputArchive oa(*m_descriptionStream);
    oa(*this);
  }

  msg.data = m_descriptionStream->ptr();
  msg.length = m_descriptionStream->tellp();
}

uint64_t
ComputeNode::Status::workQueueSize() const {
  return accumulate(solverInstances | map_values |
                      transformed(mem_fn(&SolverInstance::workQueueSize)),
                    0);
}

void
ComputeNode::Status::serializeToMessage(parac_message& msg) const {
  msg.kind = PARAC_MESSAGE_NODE_STATUS;

  if(!m_statusStream) {
    m_statusStream = std::make_unique<NoncopyOStringstream>();
  } else {
    m_statusStream->clear();
  }

  {
    cereal::BinaryOutputArchive oa(*m_statusStream);
    oa(*this);
  }

  msg.data = m_statusStream->ptr();
  msg.length = m_statusStream->tellp();
}

void
ComputeNode::incrementWorkQueueSize(parac_id originator) {
  ++m_status.solverInstances[originator].workQueueSize;
}
void
ComputeNode::decrementWorkQueueSize(parac_id originator) {
  --m_status.solverInstances[originator].workQueueSize;
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
  switch(msg.kind) {
    case PARAC_MESSAGE_NODE_DESCRIPTION:
      receiveMessageDescriptionFrom(msg);
      break;
    case PARAC_MESSAGE_NODE_STATUS: {
      receiveMessageStatusFrom(msg);
      break;
    }
    default:
      assert(false);
      break;
  }
}

void
ComputeNode::receiveMessageDescriptionFrom(parac_message& msg) {
  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg.data, msg.length);

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

  if(!m_description->daemon && !m_solverInstance) {
    auto solverMod = m_handle.modules[PARAC_MOD_SOLVER];
    assert(solverMod);
    auto solver = solverMod->solver;
    assert(solver);
    m_solverInstance = solver->add_instance(solverMod, m_node.id);
  }
}

void
ComputeNode::receiveMessageStatusFrom(parac_message& msg) {
  boost::iostreams::stream<boost::iostreams::basic_array_source<char>> data(
    msg.data, msg.length);

  {
    cereal::BinaryInputArchive ia(data);
    ia(m_status);
  }
}
}

std::ostream&
operator<<(std::ostream& o, const parac::broker::ComputeNode::Description& d) {
  return o << d.name << "@" << d.host << " " << (d.daemon ? "daemon" : "client")
           << " with " << d.workers << " available workers";
}
std::ostream&
operator<<(std::ostream& o,
           const parac::broker::ComputeNode::SolverInstance& si) {
  return o << (si.formula_parsed ? "parsed" : "unparsed") << ", "
           << si.workQueueSize << " tasks";
}
std::ostream&
operator<<(std::ostream& o, const parac::broker::ComputeNode::Status& s) {
  o << "Work queue size: " << s.workQueueSize() << ", containing "
    << s.solverInstances.size() << " instances:";
  for(auto& i : s.solverInstances) {
    o << " { " << i.first << ":" << i.second << " }";
  }
  return o;
}
