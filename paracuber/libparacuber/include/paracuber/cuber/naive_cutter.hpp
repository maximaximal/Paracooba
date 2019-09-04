#ifndef PARACUBER_CUBER_NAIVE_CUTTER_HPP
#define PARACUBER_CUBER_NAIVE_CUTTER_HPP

#include "cuber.hpp"

namespace paracuber {
namespace cuber {
class NaiveCutter : public Cuber
{
  public:
  NaiveCutter();
  virtual ~NaiveCutter();

  virtual std::shared_ptr<CNF> generateCube(std::shared_ptr<CNF> source);

  private:
};
}
}

#endif
