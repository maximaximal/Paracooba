#include "broker_compute_node.hpp"

#include <algorithm>
#include <initializer_list>
#include <numeric>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/numeric.hpp>

using boost::accumulate;
using boost::adaptors::map_values;
using boost::adaptors::transformed;
using std::bind;
using std::mem_fn;

namespace parac::broker {
ComputeNode::ComputeNode(const parac_compute_node& node)
  : m_node(node) {}
ComputeNode::~ComputeNode() {}

uint64_t
ComputeNode::Status::workQueueSize() const {
  return accumulate(solverInstances | map_values |
                      transformed(mem_fn(&SolverInstance::workQueueSize)),
                    0);
}

void
ComputeNode::initDescription(const std::string& name,
                             const std::string& host,
                             uint32_t workers,
                             uint16_t udpListenPort,
                             uint16_t tcpListenPort,
                             bool demon,
                             bool local) {
  m_description = std::make_unique<Description>(Description{
    name, host, workers, udpListenPort, tcpListenPort, demon, local });
}

void
ComputeNode::applyStatus(const Status& s) {
  m_status = s;
}
}
