#include "../../include/paracuber/cuber/registry.hpp"
#include "../../include/paracuber/cuber/cuber.hpp"

#include "../../include/paracuber/cuber/naive_cutter.hpp"

namespace paracuber {
namespace cuber {
Registry::Registry()
{
  m_cubers.push_back(std::make_unique<NaiveCutter>());
}
Registry::~Registry() {}

Cuber&
Registry::getActiveCuber()
{
  return *m_cubers[0];
}
}
}
