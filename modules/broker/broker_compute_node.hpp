#pragma once

struct parac_compute_node;

namespace parac::broker {
class ComputeNode {
  public:
  ComputeNode(const parac_compute_node& node);
  ~ComputeNode();

  private:
  const parac_compute_node& m_node;
};
}
