#include "../../include/paracuber/cuber/naive_cutter.hpp"
#include "../../include/paracuber/cnf.hpp"

namespace paracuber {
namespace cuber {
NaiveCutter::NaiveCutter() {}
NaiveCutter::~NaiveCutter() {}

std::shared_ptr<CNF>
NaiveCutter::generateCube(std::shared_ptr<CNF> source)
{
  std::shared_ptr<CNF> cube = std::make_shared<CNF>(*source);
  return std::move(cube);
}
}
}
