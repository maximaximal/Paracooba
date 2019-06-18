#ifndef PARACUBER_ORCHESTRATOR_HPP
#define PARACUBER_ORCHESTRATOR_HPP

namespace paracuber {
/** @brief Main client/node that orchestrates the solving process from
 * invocation to finish.
 *
 * The initial task to read SAT problems is handled in here. They are then
 * distributed and cubed to other solvers according to collected heuristics.
 * Actually reading the problems is done in the client, the orchestrator
 * receives a problem and owns it from then on. It also gives the final result
 * at the end.
 *
 * Multiple orchestrators exist, one for each computing node. It guides the
 * solving process and reports to its parent orchestrator, to which it
 * abstractly communicates.
 */
class Orchestrator
{
  public:
  Orchestrator();
  ~Orchestrator();

  private:
};
}

#endif
