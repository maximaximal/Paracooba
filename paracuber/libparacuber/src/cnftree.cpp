#include "../include/paracuber/cnftree.hpp"
#include <cassert>

namespace paracuber {
CNFTree::CNFTree() {}
CNFTree::~CNFTree() {}

bool
CNFTree::visit(Path p, Visitor visitor)
{
  assert(getDepth(p) < (sizeof(Path) * 8) - 6);

  Node* n = &m_root;
  uint8_t depth = 1;
  bool end = false;

  while(!end) {
    bool assignment = getAssignment(p, depth);
    CubeVar decision = n->decision;
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    if(!assignment) {
      // A left path is an inversed literal, this is a bottom assignment.
      decision = -decision;
    }

    if(n->isLeaf()) {
      end = true;
    } else {
      end = visitor(decision, depth);
    }

    if(nextPtr && depth < getDepth(p) && !n->isLeaf()) {
      n = nextPtr.get();
      ++depth;
    } else {
      end = true;
    }
  }
  return depth == getDepth(p);
}

bool
CNFTree::setDecision(Path p, CubeVar decision)
{
  assert(getDepth(p) < (sizeof(Path) * 8) - 6);

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
}
