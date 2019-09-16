#ifndef PARACUBER_CNFTREE_HPP
#define PARACUBER_CNFTREE_HPP

#include <memory>
#include <cstdint>

namespace paracuber {
class CNFTree
{
public:
  CNFTree();
  ~CNFTree();

  using Path = uint64_t;
  using CubeVar= int32_t;

  struct Node {
    std::unique_ptr<Node> left;
  };

private:
  Node root;
};
}

#endif
