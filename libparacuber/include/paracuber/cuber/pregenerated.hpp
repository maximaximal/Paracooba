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
  const messages::JobInitiator& getJobInitiator() { return m_ji; }

  virtual bool shouldGenerateTreeSplit(CNFTree::Path path);
  virtual bool getCube(CNFTree::Path path, std::vector<int>& literals);

  private:
  size_t m_counter = 0;
  messages::JobInitiator m_ji;

  const std::vector<int> *m_cubesFlatVector;
  std::vector<size_t> m_cubesJumpList;
  size_t m_normalizedPathLength = 0;
};
}
}

#endif
