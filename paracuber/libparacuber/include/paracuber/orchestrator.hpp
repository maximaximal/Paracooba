#ifndef PARACUBER_ORCHESTRATOR_HPP
#define PARACUBER_ORCHESTRATOR_HPP

namespace paracuber {
/** @brief Main client/node that orchestrates the solving process from
 * invocation to finish.
 *
 * The initial task to read SAT problems is handled in here. They are then
 * distributed and cubed to other solvers according to collected heuristics.
 */
class Orchestrator
{
  public:
  Orchestrator();
  ~Orchestrator();
};
}

#endif
