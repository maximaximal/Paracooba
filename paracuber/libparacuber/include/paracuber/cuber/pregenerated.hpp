#ifndef PARACUBER_CUBER_PREGENERATED_HPP
#define PARACUBER_CUBER_PREGENERATED_HPP

#include "../log.hpp"
#include "cuber.hpp"
#include "paracuber/messages/job_initiator.hpp"
#include <vector>

namespace paracuber {
namespace cuber {
class Pregenerated : public Cuber
{
  public:
  explicit Pregenerated(ConfigPtr config,
                        LogPtr log,
                        CNF& rootCNF,
                        const messages::JobInitiator& ji);
  virtual ~Pregenerated();

  bool init();

  virtual bool generateCube(CNFTree::Path path, CNFTree::CubeVar& var);

  private:
  size_t m_counter = 0;
  messages::JobInitiator m_ji;
};
}
}

#endif
