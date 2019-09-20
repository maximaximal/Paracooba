#ifndef PARACUBER_CNFTREE_HPP
#define PARACUBER_CNFTREE_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>

namespace paracuber {
/** @brief Implements the CNFTree concept from @ref CNFTree.
 */
class CNFTree
{
  public:
  CNFTree();
  ~CNFTree();

  using Path = uint64_t;
  using CubeVar = int32_t;

  enum StateEnum
  {
    Unvisited,
    Working,
    SAT,
    UNSAT,
    Unknown
  };
  using State = std::atomic<StateEnum>;

  /** @brief Visitor function for traversing a CNF tree.
   *
   * The CubeVar is the current decision, the uint8_t the depth.
   *
   * Return true to abort traversal, false to continue.
   */
  using Visitor = std::function<bool(CubeVar, uint8_t, State& state)>;

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

    inline bool isLeaf() { return (!left) && (!right); }
    inline bool isRemote() { return isLeaf() && remote != 0; }
  };

  /** @brief Visit the tree and call the provided visitor function.
   *
   * @return False if the path is not known, true otherwise.
   */
  bool visit(Path p, Visitor visitor);

  /** @brief Assigns a literal to the internal tree.
   *
   * The path must exist up to depth - 1 inside of this tree beforehand.
   *
   * @return False if the path does not exist yet, true if assignment was
   * successful.
   */
  bool setDecision(Path p, CubeVar decision);

  /** @brief Get the state for a given path.
   *
   * The node at the path must already exist.
   */
  bool getState(Path p, State state) const;

  /** @brief Set the state for a given path.
   *
   * The node at the path must already exist.
   */
  bool setState(Path p, State state);

  /** @brief Traverse the decision tree and write the specified path to a given
   * container.
   */
  template<class Container>
  bool writePathToLiteralContainer(Container container, Path p)
  {
    return visit(p,
                 [&container](CubeVar p, uint8_t depth, CNFTree::State& state) {
                   container.push_back(p);
                   return false;
                 });
  }
  template<class Container>
  bool writePathToLiteralAndStateContainer(Container container, Path p)
  {
    return visit(p,
                 [&container](CubeVar p, uint8_t depth, CNFTree::State& state) {
                   container.push_back({ p, state });
                   return false;
                 });
  }

  static const size_t maxPathDepth;
  static inline uint64_t getPath(Path p)
  {
    return p & (0xFFFFFFFFFFFFFFF0u | 0b11000000u);
  }
  static inline uint8_t getDepth(Path p) { return p & 0b00111111u; }
  static inline uint64_t getShiftedPath(Path p) { return p >> 6u; }
  static inline uint64_t getDepthShiftedPath(Path p)
  {
    return p >> ((sizeof(p) * 8) - getDepth(p));
  }

  static inline Path setPath(Path p, Path path)
  {
    return (p & 0b00111111) | (path & (0xFFFFFFFFFFFFFFF0u | 0b11000000u));
  }

  static inline Path setDepth(Path p, uint8_t d)
  {
    assert(d <= maxPathDepth);
    return getPath(p) | (d & 0b00111111);
  }

  template<typename T>
  static inline Path buildPath(T p, uint8_t d)
  {
    Path path = p;
    return setDepth(path << (((sizeof(Path)) - (sizeof(T))) * 8), d);
  }

  static inline bool getAssignment(Path p, uint8_t depth)
  {
    // The depth starts counting with 1 and the sizeof() is 1 too large.
    assert(depth >= 1);
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

  static inline Path down(Path p) { return setDepth(p, getDepth(p) + 1); }
  static inline Path up(Path p) { return setDepth(p, getDepth(p) - 1); }

  private:
  Node m_root;
};

template<>
inline CNFTree::Path
CNFTree::buildPath(uint64_t p, uint8_t d)
{
  return setDepth(p, d);
}

}

#endif
