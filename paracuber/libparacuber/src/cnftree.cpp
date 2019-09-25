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
  uint8_t depth = 0;
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
        return true;
      }

      break;
    }

    bool assignment = getAssignment(p, depth + 1);
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
  uint8_t depth = 0;
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
  uint8_t depth = 0;
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
void
CNFTree::pathToStr(Path p, char* str)
{
  // TODO: Make this more efficient if it is required.
  for(size_t i = 0; i < maxPathDepth; ++i) {
    if(getAssignment(p, i + 1)) {
      str[i] = '1';
    } else {
      str[i] = '0';
    }
  }
}

std::string
CNFTree::pathToStdString(Path p)
{
  char str[maxPathDepth + 1];
  pathToStr(p, str);
  return (std::string(str, getDepth(p)) + " (" + std::to_string(getDepth(p)) +
          ")");
}

bool
CNFTree::isLocal(Path p)
{
  bool isLocal = true;
  visit(p,
        [&isLocal](
          CubeVar p, uint8_t depth, CNFTree::State& state, int64_t remote) {
          if(remote != 0) {
            isLocal = false;
            return true;
          }
          return false;
        });
  return isLocal;
}

CNFTree::Path
CNFTree::strToPath(const char* str, size_t len)
{
  Path p = CNFTree::setDepth(0, len);
  for(size_t i = 0; i < len; ++i) {
    if(str[i] == '0') {
      p = setAssignment(p, i + 1, false);
    } else {
      p = setAssignment(p, i + 1, true);
    }
  }
  return p;
}
}
