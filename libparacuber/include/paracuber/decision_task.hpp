#ifndef PARACUBER_DECISIONTASK_HPP
#define PARACUBER_DECISIONTASK_HPP

#include "cnftree.hpp"
#include "task.hpp"
#include <memory>

namespace paracuber {
class CNF;

/** @brief The decision task determines the next decision literal.
 *
 * This class should only be instantiated if the decision shall be carried out
 * on the local machine. Remote decisions are decided on remote machines, after
 * beaming a path to another compute node.
 */
class DecisionTask : public Task
{
  public:
  DecisionTask(std::shared_ptr<CNF> rootCNF, CNFTree::Path p);
  virtual ~DecisionTask();

  virtual TaskResultPtr execute();
  virtual void terminate();

  CNFTree::Path getPath() const { return m_path; }

  private:
  std::shared_ptr<CNF> m_rootCNF;
  CNFTree::Path m_path;
};
}

#endif
