#ifndef PARACUBER_RUNNER_HPP
#define PARACUBER_RUNNER_HPP

namespace paracuber {
/** @brief Base class for runnable local tasks like DIMACS parsing, CDCL
 * solving, cubing, or proofing.
 *
 * This class executes tasks on a local worker thread from a local queue that is
 * detached from the Asio network code.
 */
class Runner
{
  public:
  Runner();
  ~Runner();
};
}

#endif
