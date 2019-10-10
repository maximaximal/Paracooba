#include "../include/paracuber/cnftree.hpp"
#include "../include/paracuber/communicator.hpp"
#include "../include/paracuber/config.hpp"
#include "../include/paracuber/util.hpp"
#include <cassert>
#include <iostream>

namespace paracuber {
const size_t CNFTree::maxPathDepth = sizeof(CNFTree::Path) * 8 - 6;

CNFTree::CNFTree(std::shared_ptr<Config> config, int64_t originCNFId)
  : m_config(config)
  , m_originCNFId(originCNFId)
{}
CNFTree::~CNFTree() {}

bool
CNFTree::setDecision(Path p, CubeVar decision, int64_t originator)
{
  assert(getDepth(p) < maxPathDepth);

  Node* n = &m_root;
  uint8_t depth = 0;
  bool end = false;

  while(!end) {
    if(depth == getDepth(p)) {
      // New value needs to be applied.
      n->decision = decision;
      n->remote = originator;
      if(n->isLeaf()) {
        // Create left and right branches as leaves. They have invalid decisions
        // but are required to mark the current node to have a valid decision.
        n->left = std::make_unique<Node>();
        n->right = std::make_unique<Node>();
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
    return true;
  }
  return false;
}

bool
CNFTree::getState(Path p, State& state) const
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
      state = n->state;
      return true;
    }

    bool assignment = getAssignment(p, depth + 1);
    const std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    n = nextPtr.get();
    ++depth;
  }
  return false;
}
bool
CNFTree::getDecision(Path p, CubeVar& var) const
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
      var = n->decision;
      return true;
    }

    bool assignment = getAssignment(p, depth + 1);
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

  Node *n = &m_root, *sibling = nullptr;
  uint8_t depth = 0;
  bool end = false;

  while(!end) {
    if(!n) {
      return false;
    }

    if(depth == getDepth(p)) {
      n->state = state;

      if(n->remote != 0) {
        // This change must be propagated to the remote compute node! This is
        // done through the communicator class.
        std::shared_ptr<CNF> rootCNF =
          GetRootCNF(m_config.get(), m_originCNFId);
        assert(rootCNF);
        auto& statNode =
          m_config->getCommunicator()->getClusterStatistics()->getNode(
            n->remote);
        m_config->getCommunicator()->sendCNFResultToNode(
          rootCNF, p, statNode.getNetworkedNode());
      }
      break;
    }

    bool assignment = getAssignment(p, depth + 1);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;
    std::unique_ptr<Node>& siblingPtr = !assignment ? n->left : n->right;

    n = nextPtr.get();
    sibling = siblingPtr.get();
    ++depth;
  }

  switch(state) {
    case SAT:
      // A SAT assignment must directly be propagated upwards.
      if(getDepth(p) > 0) {
        setState(setDepth(p, getDepth(p) - 1), SAT);
      }
      break;
    case UNSAT:
      // Only if the other sibling is also UNSAT, this must be propagated.
      if(sibling) {
        if(sibling->state == UNSAT) {
          setState(setDepth(p, getDepth(p) - 1), UNSAT);
        }
      }
      break;
    default:
      // No other cases covered.
      break;
  }

  return depth == getDepth(p);
}

bool
CNFTree::setRemote(Path p, int64_t remote)
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
      n->remote = remote;
      return true;
    }

    bool assignment = getAssignment(p, depth + 1);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    n = nextPtr.get();
    ++depth;
  }
  return false;
}

bool
CNFTree::setDecisionAndState(Path p, CubeVar decision, State state)
{
  assert(getDepth(p) < maxPathDepth);

  Node* n = &m_root;
  uint8_t depth = 0;
  bool end = false;

  while(!end) {
    if(depth == getDepth(p)) {
      n->decision = decision;
      n->state = state;
      if(n->isLeaf()) {
        // New value needs to be applied.

        // Create left and right branches as leaves. They have invalid decisions
        // but are required to mark the current node to have a valid decision.
        if(!n->left) {
          n->left = std::make_unique<Node>();
        }
        if(!n->right) {
          n->right = std::make_unique<Node>();
        }
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

void
CNFTree::pathToStr(Path p, char* str)
{
  // TODO: Make this more efficient if it is required.
  for(size_t i = 0; i < maxPathDepth; ++i) {
    str[i] = getAssignment(p, i + 1) + '0';
  }
  str[getDepth(p)] = '\0';
}
const char*
CNFTree::pathToStrNoAlloc(Path p)
{
  static thread_local char arr[maxPathDepth];
  pathToStr(p, arr);
  return arr;
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
  visit(
    p,
    [&isLocal](CubeVar p, uint8_t depth, CNFTree::State state, int64_t remote) {
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
std::ostream&
operator<<(std::ostream& o, CNFTree::StateEnum s)
{
  o << CNFTreeStateToStr(s);
  return o;
}
}
