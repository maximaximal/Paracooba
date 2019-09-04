#ifndef PARACUBER_CUBER_REGISTRY_HPP
#define PARACUBER_CUBER_REGISTRY_HPP

#include <memory>
#include <vector>

namespace paracuber {
namespace cuber {
class Cuber;

class Registry
{
  public:
  Registry();
  ~Registry();

  using CuberVector = std::vector<std::unique_ptr<Cuber>>;

  Cuber& getActiveCuber();

  private:
  CuberVector m_cubers;
};
}
}

#endif
