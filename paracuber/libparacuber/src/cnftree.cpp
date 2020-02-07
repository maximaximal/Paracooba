#include "../include/paracuber/cnftree.hpp"
#include "../include/paracuber/cnf.hpp"
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
      if(n->isLeaf()) {
        // Create left and right branches as leaves. They have invalid decisions
        // but are required to mark the current node to have a valid decision.
        n->left = std::make_unique<Node>();
        n->left->remote = originator;
        n->right = std::make_unique<Node>();
        n->right->remote = originator;
        return true;
      }

      break;
    }

    bool assignment = getAssignment(p, depth + 1);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

    if(!nextPtr) {
      nextPtr = std::make_unique<Node>();
      nextPtr->remote = originator;
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
CNFTree::setState(Path p, State state, bool keepLocal)
{
  assert(getDepth(p) < maxPathDepth);

  Node *n = &m_root, *sibling = nullptr, *lastNode = nullptr;
  uint8_t depth = 0;
  bool end = false;

  while(!end) {
    if(!n) {
      return false;
    }

    if(depth == getDepth(p)) {
      n->state = state;
      break;
    }

    bool assignment = getAssignment(p, depth + 1);
    std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;
    std::unique_ptr<Node>& siblingPtr = !assignment ? n->left : n->right;

    lastNode = n;
    sibling = siblingPtr.get();
    n = nextPtr.get();
    ++depth;
  }

  if(getDepth(p) > 0) {
    assert(sibling);

    switch(state) {
      case SAT:
        // A SAT assignment must directly be propagated upwards this depends on
        // the parent being remote or not. If the parent is remote, send the
        // result to the parent. If not, keep it directly local.
        setCNFResult(getParent(p), SAT, p);
        if(lastNode->isLocal()) {
          setState(getParent(p), SAT);
        } else {
          sendPathToRemote(getParent(p), lastNode);
        }
        break;
      case UNSAT: {
        if(lastNode->isLocal()) {
          // This applies to cases 1 and 2.
          //
          // Case 1:
          //   The parent (local) will receive the UNSAT result once both
          //   sibling nodes set their states.
          //
          // Case 2:
          //   The parent (local) will get the UNSAT result, once the sibling
          //   sends the result over the network and lands in the sibling
          //   node. The sibling setState() call then propagates the result
          //   upwards.
          if(sibling->state == UNSAT) {
            // Derive new CNF result for parent path!
            setCNFResult(getParent(p), UNSAT, p);
            setState(getParent(p), UNSAT);
          }
        } else {
          // This applies to cases 3 and 4.
          //
          // Case 3:
          //   The parent (remote) must know about this UNSAT result only if the
          //   sibling is also UNSAT. This node collects the result from both
          //   siblings and only sends the finished notification once both are
          //   UNSAT.
          //
          // Case 4:
          //   The parent (remote) must know about this result, as the sibling
          //   is also remote and will send its result to the mutual parent.
          //   This means, this result must directly be sent to the parent.
          if(sibling->isLocal()) {
            // Case 3
            if(sibling->state == UNSAT) {
              // Derive new CNF result for parent path!
              setCNFResult(getParent(p), UNSAT, p);
              sendPathToRemote(getParent(p), lastNode);
            } else {
              // Not handled in this context, once the sibling finishes, this
              // will come from the other side.
            }
          } else {
            // Case 4
            sendPathToRemote(p, lastNode);
          }
        }
        break;
      }
      default:
        // No other cases covered.
        break;
    }
  }

  if(depth == getDepth(p)) {
    return true;
  }
  return false;
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
  if(getDepth(p) > maxPathDepth)
    return "INVALID PATH";
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

void
CNFTree::sendPathToRemote(Path p, Node* n)
{
  assert(n);
  std::shared_ptr<CNF> rootCNF = GetRootCNF(m_config.get(), m_originCNFId);
  assert(rootCNF);
  auto& statNode =
    m_config->getCommunicator()->getClusterStatistics()->getNode(n->remote);
  rootCNF->sendResult(statNode.getNetworkedNode(), p, []() {});
}

void
CNFTree::setCNFResult(Path p, State state, Path source)
{
  std::shared_ptr<CNF> rootCNF = GetRootCNF(m_config.get(), m_originCNFId);
  rootCNF->insertResult(p, state, source);
  signalIfRootStateChanged(p, state);
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
