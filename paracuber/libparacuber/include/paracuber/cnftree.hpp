#ifndef PARACUBER_CNFTREE_HPP
#define PARACUBER_CNFTREE_HPP

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

  /** @brief Visitor function for traversing a CNF tree.
   *
   * The CubeVar is the current decision, the uint8_t the depth.
   *
   * Return true to abort traversal, false to continue.
   */
  using Visitor = std::function<bool(CubeVar, uint8_t)>;

  /** @brief A single node on the cubing binary tree.
   *
   * A leaf (as given by isLeaf()) has no valid decision literal. All other
   * nodes have valid decision literals.
   */
  struct Node
  {
    CubeVar decision;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    inline bool isLeaf() { return (!left) && (!right); }
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

  /** @brief Traverse the decision tree and write the specified path to a given
   * container.
   */
  template<class Container>
  bool writePathToContainer(Container container, Path p)
  {
    return visit(p, [&container](CubeVar p, uint8_t depth) {
      container.push_back(p);
      return false;
    });
  }

  static inline uint64_t getPath(Path p)
  {
    return p & (0xFFFFFFFFFFFFFFF0u | 0b11000000u);
  }
  static inline uint8_t getDepth(Path p) { return p & 0b00111111u; }

  static inline Path setPath(Path p, Path path)
  {
    return (p & 0b00111111) | (path & (0xFFFFFFFFFFFFFFF0u | 0b11000000u));
  }

  static inline Path setDepth(Path p, uint8_t d)
  {
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
    return p &
           (static_cast<Path>(1u) << ((sizeof(Path) * 8) - 1 - (depth - 1)));
  }

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
