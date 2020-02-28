#ifndef PARACOOBA_PRIORITYQUEUELOCKSEMANTICS_HPP
#define PARACOOBA_PRIORITYQUEUELOCKSEMANTICS_HPP

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <type_traits>

namespace paracooba {
template<typename T, class C = std::less<T>>
class PriorityQueueLockSemantics
{
  public:
  PriorityQueueLockSemantics(C compare = C())
    : m_compare(compare)
  {}
  ~PriorityQueueLockSemantics() {}

  void pushNoLock(T obj)
  {
    m_queue.push_back(std::move(obj));
    std::sort(m_queue.begin(), m_queue.end(), m_compare);
  }

  void push(T obj)
  {
    std::unique_lock lock(m_mutex);
    pushNoLock(std::move(obj));
  }

  T popNoLock()
  {
    auto result = std::move(m_queue.front());
    m_queue.pop_front();
    return result;
  }

  T pop()
  {
    std::unique_lock lock(m_mutex);
    return popNoLock();
  }

  T popBackNoLock()
  {
    auto result = std::move(m_queue.back());
    m_queue.pop_back();
    return result;
  }

  T popBack()
  {
    std::unique_lock lock(m_mutex);
    return popBackNoLock();
  }

  template<class Predicate>
  void removeMatchingNoLock(Predicate p)
  {
    auto toBeRemoved = std::remove_if(m_queue.begin(), m_queue.end(), p);
    m_queue.erase(toBeRemoved, m_queue.end());
  }

  template<class Predicate>
  void removeMatching(Predicate p)
  {
    std::unique_lock lock(m_mutex);
    removeMatchingNoLock(p);
  }

  inline bool empty() const { return m_queue.empty(); }

  inline int64_t size() const { return m_queue.size(); }

  inline std::mutex& getMutex() { return m_mutex; }

  private:
  std::deque<T> m_queue;
  std::mutex m_mutex;
  C m_compare;
};

template<typename T>
struct unique_ptr_less
{
  inline int operator()(const std::unique_ptr<T>& l,
                        const std::unique_ptr<T>& r)
  {
    if(l && !r)
      return 0;
    if(!l && r)
      return -1;
    if(!l && !r)
      return 0;
    return *l < *r;
  }
};

template<typename T>
using PriorityQueueLockSemanticsUniquePtr =
  PriorityQueueLockSemantics<std::unique_ptr<T>, unique_ptr_less<T>>;
}

#endif
