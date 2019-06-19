#ifndef PARACUBER_TASK_HPP
#define PARACUBER_TASK_HPP

#include "taskresult.hpp"

namespace paracuber {
/** @brief Environment for tasks anywhere in the distributed system.
 *
 * This must be sub-classed by actual tasks to be run.
 */
class Task
{
  public:
  /** @brief Constructor */
  Task();
  /** @brief Destructor */
  virtual ~Task();

  /** @brief Execute this task.
   *
   * Must be implemented by actual tasks. May be called multiple times to re-use
   * old task objects.
   * */
  virtual TaskResultPtr execute() = 0;

  private:
};
}

#endif
