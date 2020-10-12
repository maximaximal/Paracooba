#include "broker_compute_node.hpp"

namespace parac::broker {
ComputeNode::ComputeNode(const parac_compute_node& node)
  : m_node(node) {}
ComputeNode::~ComputeNode() {}
}
