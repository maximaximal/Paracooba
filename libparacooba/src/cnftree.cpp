#include "../include/paracooba/cnftree.hpp"
#include "../include/paracooba/cnf.hpp"
#include "../include/paracooba/communicator.hpp"
#include "../include/paracooba/config.hpp"
#include "../include/paracooba/networked_node.hpp"
#include "../include/paracooba/util.hpp"

#include "../include/paracooba/messages/message_receiver.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace paracooba {
const size_t CNFTree::maxPathDepth = sizeof(Path) * 8 - 6;

// Taken from https://stackoverflow.com/a/45507610/2646647
template<typename T>
bool
isUninitialized(std::weak_ptr<T> const& weak)
{
  using wt = std::weak_ptr<T>;
  return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}

template<typename T>
std::string
ptrToString(std::weak_ptr<T> const& w, const std::string& fallback)
{
  if(isUninitialized(w))
    return fallback;

  auto val = w.lock();
  std::stringstream sstream;
  sstream << *val;

  return sstream.str();
}

CNFTree::CNFTree(LogPtr log,
                 CNF& rootCNF,
                 std::shared_ptr<Config> config,
                 int64_t originCNFId)
  : m_rootCNF(rootCNF)
  , m_config(config)
  , m_originCNFId(originCNFId)
  , m_logger(log->createLogger(
      ("CNFTree of formula " + std::to_string(rootCNF.getOriginId()))))
{}
CNFTree::~CNFTree() {}

bool
CNFTree::Node::isOffloaded() const
{
  return !isLocal();
}
bool
CNFTree::Node::isLocal() const
{
  return isUninitialized(offloadedTo);
}
bool
CNFTree::Node::requiresRemoteUpdate() const
{
  return !isUninitialized(receivedFrom);
}

void
CNF::initMeanDuration(size_t windowSize)
{
  using namespace std::chrono_literals;
  for(size_t i = 0; i < windowSize; ++i) {
    m_acc_solvingTime(1'500ms);
  }
}

CNFTree::State
CNFTree::getState(Path p) const
{
  std::lock_guard lock(m_nodeMapMutex);
  assert(getDepth(p) < maxPathDepth);
  const Node* node = getNode(p);
  if(node)
    return node->state;
  return State::UnknownPath;
}

void
CNFTree::setStateFromLocal(Path p, State state)
{
  std::lock_guard lock(m_nodeMapMutex);
  Node* node = getNode(p);
  if(!node) {
    node =
      m_nodeMap.insert(std::make_pair(cleanupPath(p), std::make_unique<Node>()))
        .first->second.get();
  }

  State stateBefore = node->state;
  node->state = state;

  if(getDepth(p) == 0 && node->state != stateBefore) {
    m_rootStateChangedSignal(p, state);
    return;
  }

  m_rootCNF.insertResult(cleanupPath(p), state, DefaultUninitiatedPath);

  propagateUpwardsFrom(p);
}
void
CNFTree::setStateFromRemote(Path p, State state, NetworkedNode& remoteNode)
{
  std::lock_guard lock(m_nodeMapMutex);
  Node* node = getNode(p);
  assert(node);
  // TODO readd assert(!node->offloadedTo.expired() && *node->offloadedTo.lock()
  // == remoteNode);
  node->state = state;

  propagateUpwardsFrom(p);
}
void
CNFTree::insertNodeFromRemote(Path p, std::shared_ptr<NetworkedNode> remoteNode)
{
  std::lock_guard lock(m_nodeMapMutex);
  auto [it, inserted] =
    m_nodeMap.insert(std::make_pair(cleanupPath(p), std::make_unique<Node>()));
  Node* node = it->second.get();
  if(!inserted) {
    // Node is offloaded again to the same remote node. This can basically only
    // happen if a node went offline, sent a status update, went online again
    // immediately and received the same job as before. If this event is just
    // silently ignored and the receivedFrom field replaced, the result will
    // still be propagated upwards correctly.
    {
      std::unique_lock loggerLock(m_logMutex);
      PARACOOBA_LOG(m_logger, GlobalWarning)
        << "Receive path " << pathToStrNoAlloc(p)
        << " that was inserted previously from remote "
        << ptrToString(node->receivedFrom, "Local") << " again! This time from "
        << remoteNode->getId()
        << ". Setting receivedFrom to new remote and ignore this "
           "network-related "
           "error.";
    }
  }
  node->receivedFrom = remoteNode;
}
void
CNFTree::offloadNodeToRemote(Path p, std::shared_ptr<NetworkedNode> remoteNode)
{
  std::lock_guard lock(m_nodeMapMutex);
  Node* node = getNode(p);
  if(!node) {
    node =
      m_nodeMap.insert(std::make_pair(cleanupPath(p), std::make_unique<Node>()))
        .first->second.get();
  }
  assert(!node->isOffloaded());

  node->offloadedTo = remoteNode;
  node->state = Working;
}
void
CNFTree::resetNode(Path p)
{
  std::lock_guard lock(m_nodeMapMutex);
  Node* node = getNode(p);
  assert(node);
  assert(node->isOffloaded());
  node->offloadedTo.reset();
  node->state = Unvisited;
}

std::shared_ptr<NetworkedNode>
CNFTree::getOffloadTargetNetworkedNode(Path p)
{
  std::lock_guard lock(m_nodeMapMutex);
  Node* node = getNode(p);
  if(!node) {
    return nullptr;
  }
  if(node->isOffloaded()) {
    if(auto nn = node->offloadedTo.lock())
      return nn;
    else
      return nullptr;
  }
  return nullptr;
}

Path
CNFTree::getTopmostAvailableParent(Path p) const
{
  std::lock_guard lock(m_nodeMapMutex);
  return getTopmostAvailableParentInner(p);
}
Path
CNFTree::getTopmostAvailableParentInner(Path p) const
{
  const Node* n = getNode(p);
  if(!n)
    return DefaultUninitiatedPath;

  if(getDepth(p) == 0)
    return p;

  Path parentPath = getParent(p);
  const Node* parentNode = getNode(parentPath);
  if(parentNode->requiresRemoteUpdate()) {
    return parentPath;
  }
  return getTopmostAvailableParentInner(parentPath);
}

void
CNFTree::propagateUpwardsFrom(Path p, Path sourcePath)
{
  // Starts at a node, but must immediately go to parent if called with invalid
  // source path. This is only the first call, from functions above.
  if(sourcePath == DefaultUninitiatedPath) {
    if(getDepth(p) == 0)
      return;

    {
      // The node that has received an update could directly require sending it
      // to the remote!
      Node* node = getNode(p);
      if(node->requiresRemoteUpdate() &&
         (node->state == SAT || node->state == UNSAT)) {
        sendNodeResultToSender(p, *node);
      }
    }

    sourcePath = p;
    p = getParent(p);
  }

  p = cleanupPath(p);

  Node* node = getNode(p);
  if(!node) {
    assert(getDepth(p) < maxPathDepth);
    // The node must always exist, except work was directly offloaded.
    assert(
      (getNode(sourcePath) && getNode(sourcePath)->requiresRemoteUpdate()));
    std::unique_lock loggerLock(m_logMutex);
    PARACOOBA_LOG(m_logger, Trace) << "Directly offloaded "
                                   << "for path " << pathToStdString(p);
    return;
  }

  bool changed = false;

  const Node* leftChild = getNode(getNextLeftPath(p));
  const Node* rightChild = getNode(getNextRightPath(p));

  if(leftChild && leftChild->state == UNSAT && rightChild &&
     rightChild->state == UNSAT) {
    node->state = UNSAT;
    changed = true;
  }

  if((leftChild && leftChild->state == SAT) ||
     (rightChild && rightChild->state == SAT)) {
    // don't bubble up the results twice
    changed = node ? node->state != SAT : false;
    node->state = SAT;
  }

  if(changed) {
    {
      std::unique_lock loggerLock(m_logMutex);
      PARACOOBA_LOG(m_logger, Trace)
        << "Deduce " << node->state << " for path " << pathToStdString(p)
        << " when applying path " << pathToStdString(sourcePath);
    }

    setCNFResult(p, node->state, sourcePath);

    if(getDepth(p) == 0) {
      m_rootStateChangedSignal(p, node->state);
      return;
    }

    if(node->requiresRemoteUpdate()) {
      sendNodeResultToSender(p, *node);
    } else {
      propagateUpwardsFrom(getParent(p), p);
    }
  }
}

void
CNFTree::sendNodeResultToSender(Path p, const Node& node)
{
  assert(node.requiresRemoteUpdate());
  if(auto nn = node.receivedFrom.lock())
    m_rootCNF.sendResult(*nn, cleanupPath(p), []() {});
  else {
    PARACOOBA_LOG(m_logger, GlobalError)
      << "Could not send result for path " << CNFTree::pathToStrNoAlloc(p)
      << " back to remote!";
  }
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
  if(p == DefaultUninitiatedPath)
    return "(nowhere)";
  if(p == 0)
    return "(root)";
  if(getDepth(p) > maxPathDepth)
    return "INVALID PATH";
  static thread_local char arr[maxPathDepth];
  pathToStr(p, arr);
  return arr;
}

std::string
CNFTree::pathToStdString(Path p)
{
  const char* str = pathToStrNoAlloc(p);
  return (std::string(str) + " (" + std::to_string(getDepth(p)) + ")");
}

void
CNFTree::dumpTreeToFile(const std::string_view& file)
{
  std::ofstream outFile;
  outFile.open(std::string(file));
  if(outFile.is_open()) {
    outFile << "digraph ParacoobaTree {";
    bool limitedDump = m_config->isLimitedTreeDumpActive();
    std::string rootParentPath = "";
    std::set<Path> visitedPaths;
    for(const auto& it : m_nodeMap) {
      dumpNode(it.first,
               it.second.get(),
               visitedPaths,
               outFile,
               limitedDump,
               rootParentPath);
    }
    outFile << "}" << std::endl;
    outFile.close();
  }
}
void
CNFTree::dumpNode(Path p,
                  const Node* n,
                  PathSet& visitedPaths,
                  std::ostream& o,
                  bool limitedDump,
                  const std::string& parentPath)
{
  if(!n || !visitedPaths.insert(cleanupPath(p)).second)
    return;

  if(limitedDump && (n->state == SAT || n->state == UNSAT))
    return;

  std::string pStr = "n";
  pStr += CNFTree::pathToStrNoAlloc(p);
  o << pStr << " [label=\"" << pStr << "(" << n->state << ") >";
  if(auto nn = n->receivedFrom.lock())
    o << "no_remote";
  else
    o << n->receivedFrom.lock();
  o << ">";
  if(auto nn = n->offloadedTo.lock())
    o << "no_remote";
  else
    o << n->receivedFrom.lock();

  o << "\" shape=box];" << std::endl;

  if(parentPath != "") {
    o << parentPath << " -> " << pStr << ";" << std::endl;
  }

  {
    Path left = getNextLeftPath(p);
    dumpNode(left, getNode(left), visitedPaths, o, limitedDump, pStr);
  }
  {
    Path right = getNextRightPath(p);
    dumpNode(right, getNode(right), visitedPaths, o, limitedDump, pStr);
  }
}

void
CNFTree::setCNFResult(Path p, State state, Path source)
{
  m_rootCNF.insertResult(cleanupPath(p), state, cleanupPath(source));
}

Path
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
operator<<(std::ostream& o, CNFTree::State s)
{
  o << CNFTreeStateToStr(s);
  return o;
}
}
