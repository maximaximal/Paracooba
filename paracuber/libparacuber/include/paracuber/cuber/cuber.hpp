#ifndef PARACUBER_CUBER_CUBER_HPP
#define PARACUBER_CUBER_CUBER_HPP

#include <memory>

namespace paracuber {
class CNF;

namespace cuber {

/** @brief Base class for all cubing algorithms.
 *
 * This class is initialised once per formula (daemon and client), so the
 * algorithm can track internal statistics to improve cube selection if desired.
 */
class Cuber
{
  public:
  Cuber() {}
  virtual ~Cuber() {}

  /** @brief Generates the next cube out of a given source.
   *
   * Required method for implementing cubing algorithms.
   */
  virtual std::shared_ptr<CNF> generateCube(std::shared_ptr<CNF> source) = 0;

  private:
};
}
}

#endif
