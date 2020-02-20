#ifndef PARACUBER_CNFTREE_HPP
#define PARACUBER_CNFTREE_HPP

#include <atomic>
#include <boost/signals2/signal.hpp>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

namespace paracuber {
class Config;
class CNF;

/** @brief Implements the CNFTree concept from @ref CNFTree.
 */
class CNFTree
{
  public:
  explicit CNFTree(std::shared_ptr<Config> config, int64_t originCNFId);
  ~CNFTree();

  using Path = uint64_t;
  using CubeVar = int32_t;

  static const Path DefaultUninitiatedPath = std::numeric_limits<Path>::max();

  enum StateEnum
  {
    Unvisited,
    Working,
    Solving,
    Split,
    SAT,
    UNSAT,
    Dropped,
    Unknown,
    _STATE_COUNT
  };
  using State = StateEnum;
  using StateChangedSignal = boost::signals2::signal<void(Path, StateEnum)>;

  /** @brief Visitor function for traversing a CNF tree.
   *
   * The CubeVar is the current decision, the uint8_t the depth.
   *
   * Return true to abort traversal, false to continue.
   *
   * using VisitorFunc = std::function<bool(CNFTree::CubeVar, uint8_t, State
   * state, int64_t remote)>;
   */

  /** @brief A single node on the cubing binary tree.
   *
   * A leaf (as given by isLeaf()) has no valid decision literal. All other
   * nodes have valid decision literals, except for remote ones, where remote !=
   * 0.
   *
   * Remote nodes are leaves where the decision is != 0. These nodes are
   * materialised on other compute nodes.
   */
  struct Node
  {
    CubeVar decision = 0;
    State state = Unvisited;
    int64_t remote = 0;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    inline bool isLeaf() const { return (!left) && (!right); }
    inline bool isRemote() const { return remote != 0; }
    inline bool isLocal() const { return !isRemote(); }
  };

  /** @brief Visit the tree and call the provided visitor function.
   *
   * @return False if the path is not known, true otherwise.
   */
  template<class Visitor>
  bool visit(Path p, Visitor visitor) const
  {
    assert(getDepth(p) < maxPathDepth);

    const Node* n = &m_root;
    uint8_t depth = 0;
    bool end = false;

    while(!end) {
      bool assignment = getAssignment(p, depth + 1);
      CubeVar decision = n->decision;
      const std::unique_ptr<Node>& nextPtr = assignment ? n->left : n->right;

      if(!assignment) {
        // A left path is an inversed literal, this is a bottom assignment.
        decision = -decision;
      }

      if(n->isLeaf()) {
        end = true;
      } else {
        end = visitor(decision, depth, n->state, n->remote);
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

  void dumpTreeToFile(const std::string_view& targetFile);

  bool isLocal(Path p);

  /** @brief Assigns a literal to the internal tree.
   *
   * The path must exist up to depth - 1 inside of this tree beforehand.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool setDecision(Path p, CubeVar decision, int64_t originator = 0);

  /** @brief Get the state for a given path.
   *
   * The node at the path must already exist.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool getState(Path p, State& state) const;

  /** @brief Get the decision for a given path.
   *
   * The node at the path must already exist.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool getDecision(Path p, CubeVar& var) const;

  /** @brief Set the state for a given path.
   *
   * The node at the path must already exist.
   *
   * By keeping a state change local, no network call is made.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool setState(Path p, State state, bool keepLocal = false);

  /** @brief Set the remote for a given path.
   *
   * The node at the path must already exist.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool setRemote(Path p, int64_t remote);

  /** @brief Bulk-set a node.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool setDecisionAndState(Path p, CubeVar decision, State state);

  /** @brief This signal is called when the root state changes.
   */
  StateChangedSignal& getRootStateChangedSignal()
  {
    return m_rootStateChangedSignal;
  }

  /** @brief Traverse the decision tree and write the specified path to a given
   * container.
   */
  template<class Container>
  bool writePathToLiteralContainer(Container& container, Path p)
  {
    return visit(
      p,
      [&container](
        CubeVar decision, uint8_t depth, CNFTree::State state, int64_t remote) {
        container.push_back(decision);
        return false;
      });
  }
  template<class Container>
  bool writePathToLiteralAndStateContainer(Container& container, Path p)
  {
    return visit(
      p,
      [&container](
        CubeVar p, uint8_t depth, CNFTree::State state, int64_t remote) {
        container.push_back({ p, state });
        return false;
      });
  }

  static const size_t maxPathDepth;
  static inline uint64_t getPath(Path p)
  {
    return p & (0xFFFFFFFF'FFFFFF00u | 0b11000000u);
  }
  static inline uint8_t getDepth(Path p) { return p & 0b00111111u; }
  static inline uint64_t getShiftedPath(Path p) { return p >> 6u; }
  static inline uint64_t getDepthShiftedPath(Path p)
  {
    return p >> ((sizeof(p) * 8) - getDepth(p));
  }

  static inline Path setPath(Path p, Path path)
  {
    return (p & 0b00111111u) | (path & (0xFFFFFFFF'FFFFFF00u | 0b11000000u));
  }

  static inline Path setDepth(Path p, uint8_t d)
  {
    assert(d <= maxPathDepth);
    return getPath(p) | (d & 0b00111111u);
  }

  static inline Path getParent(Path p)
  {
    assert(getDepth(p) >= 1);
    return setDepth(p, getDepth(p) - 1);
  }

  static void pathToStr(Path p, char* str);
  static const char* pathToStrNoAlloc(Path p);
  static std::string pathToStdString(Path p);
  static Path strToPath(const char* str, size_t len);

  template<typename T>
  static inline Path buildPath(T p, uint8_t d)
  {
    Path path = p;
    return setDepth(path << (((sizeof(Path)) - (sizeof(T))) * 8), d);
  }

  static inline bool getAssignment(Path p, uint8_t depth)
  {
    // The depth starts counting with 1 and the sizeof() is 1 too large.
    assert(depth <= maxPathDepth);
    return p &
           (static_cast<Path>(1u) << ((sizeof(Path) * 8) - 1 - (depth - 1)));
  }

  static inline Path setAssignment(Path p, uint8_t depth, bool val)
  {
    // The depth starts counting with 1 and the sizeof() is 1 too large.
    assert(depth >= 1);
    assert(depth <= maxPathDepth);
    return (p & ~(static_cast<Path>(1u)
                  << ((sizeof(Path) * 8) - 1 - (depth - 1)))) |
           (static_cast<Path>(val) << ((sizeof(Path) * 8) - 1 - (depth - 1)));
  }

  static inline Path getNextLeftPath(Path p)
  {
    uint8_t depth = getDepth(p) + 1;
    return setDepth(setAssignment(p, depth, false), depth);
  }
  static inline Path getNextRightPath(Path p)
  {
    uint8_t depth = getDepth(p) + 1;
    return setDepth(setAssignment(p, depth, true), depth);
  }

  static inline Path down(Path p) { return setDepth(p, getDepth(p) + 1); }
  static inline Path up(Path p) { return setDepth(p, getDepth(p) - 1); }

  private:
  Node m_root;
  std::shared_ptr<Config> m_config;
  int64_t m_originCNFId;
  StateChangedSignal m_rootStateChangedSignal;

  void sendPathToRemote(Path p, Node* n);
  void setCNFResult(Path p, State state, Path source);

  inline void signalIfRootStateChanged(Path p, State s)
  {
    if(m_root.state != s && getDepth(p) == 0 &&
       (s == SAT || s == UNSAT || s == Unknown))
      m_rootStateChangedSignal(p, s);
  }
};

constexpr const char*
CNFTreeStateToStr(CNFTree::StateEnum s)
{
  switch(s) {
    case CNFTree::StateEnum::Unvisited:
      return "Unvisited";
    case CNFTree::StateEnum::Working:
      return "Working";
    case CNFTree::StateEnum::SAT:
      return "SAT";
    case CNFTree::StateEnum::UNSAT:
      return "UNSAT";
    case CNFTree::StateEnum::Solving:
      return "Solving";
    case CNFTree::StateEnum::Split:
      return "Split";
    case CNFTree::StateEnum::Dropped:
      return "Dropped";
    case CNFTree::StateEnum::Unknown:
      return "Unknown";
    default:
      return "Unknown State";
  }
}

std::ostream&
operator<<(std::ostream& o, CNFTree::StateEnum s);

template<>
inline CNFTree::Path
CNFTree::buildPath(uint64_t p, uint8_t d)
{
  return setDepth(p, d);
}

}

#endif
