#include "../include/paracuber/cnftree.hpp"
#include <cassert>

namespace paracuber {
const size_t CNFTree::maxPathDepth = sizeof(CNFTree::Path) * 8 - 6;

CNFTree::CNFTree() {}
CNFTree::~CNFTree() {}

bool
CNFTree::setDecision(Path p, CubeVar decision)
{
  assert(getDepth(p) < maxPathDepth);

  Node* n = &m_root;
  uint8_t depth = 1;
  bool end = false;

  while(!end) {
    if(depth == getDepth(p)) {
      if(!n->isLeaf() && n->decision != decision) {
        /// The decision is invalid if a child node already exists, or if it
        /// conflicts with the old assignment.
        return false;
      }
      if(n->isLeaf()) {
        // New value needs to be applied.

        // Create left and right branches as leaves. They have invalid decisions
        // but are required to mark the current node to have a valid decision.
        n->left = std::make_unique<Node>();
        n->right = std::make_unique<Node>();
        n->decision = decision;
      }

      break;
    }

    bool assignment = getAssignment(p, depth);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    if(!nextPtr) {
      nextPtr = std::make_unique<Node>();
    }

    n = nextPtr.get();
    ++depth;
  }

  if(depth == getDepth(p)) {
    n->decision = decision;
    return true;
  }
  return false;
}

bool
CNFTree::getState(Path p, State state) const
{
  assert(getDepth(p) < maxPathDepth);

  const Node* n = &m_root;
  uint8_t depth = 1;
  bool end = false;

  while(!end) {
    if(!n) {
      return false;
    }

    if(depth == getDepth(p)) {
      state = n->state.load();
      return true;
    }

    bool assignment = getAssignment(p, depth);
    const std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    n = nextPtr.get();
    ++depth;
  }
  return false;
}

bool
CNFTree::setState(Path p, State state)
{
  assert(getDepth(p) < maxPathDepth);

  Node* n = &m_root;
  uint8_t depth = 1;
  bool end = false;

  while(!end) {
    if(!n) {
      return false;
    }

    if(depth == getDepth(p)) {
      n->state.store(state);
      return true;
    }

    bool assignment = getAssignment(p, depth);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    n = nextPtr.get();
    ++depth;
  }
  return false;
}
}
