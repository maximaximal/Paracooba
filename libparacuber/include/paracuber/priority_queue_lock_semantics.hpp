#ifndef PARACUBER_PRIORITYQUEUELOCKSEMANTICS_HPP
#define PARACUBER_PRIORITYQUEUELOCKSEMANTICS_HPP

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace paracuber {
template<typename T, class C = std::less<T>>
class PriorityQueueLockSemantics
{
  public:
  PriorityQueueLockSemantics(size_t size = 0, C compare = C())
    : m_queue(size)
    , m_compare(compare)
  {}
  ~PriorityQueueLockSemantics() {}

  void pushNoLock(T obj)
  {
    ++m_size;
    m_queue.push_back(std::move(obj));
    std::push_heap(m_queue.begin(), m_queue.end(), m_compare);
  }

  void push(T obj)
  {
    std::unique_lock lock(m_mutex);
    pushNoLock(std::move(obj));
  }

  T popNoLock()
  {
    --m_size;
    auto result = std::move(m_queue.front());
    std::pop_heap(m_queue.begin(), m_queue.end(), m_compare);
    m_queue.pop_back();
    return result;
  }

  T pop()
  {
    std::unique_lock lock(m_mutex);
    return popNoLock();
  }

  T popBackNoLock()
  {
    --m_size;
    auto result = std::move(*(m_queue.begin() + m_size));
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
    m_size -= std::distance(toBeRemoved, m_queue.end());
    m_queue.erase(toBeRemoved, m_queue.end());
  }

  template<class Predicate>
  void removeMatching(Predicate p)
  {
    std::unique_lock lock(m_mutex);
    removeMatchingNoLock(p);
  }

  inline bool empty() const { return m_size == 0; }

  inline int64_t size() const { return m_size; }

  inline std::mutex& getMutex() { return m_mutex; }

  private:
  std::vector<T> m_queue;
  std::atomic_int64_t m_size = 0;
  std::mutex m_mutex;
  C m_compare;
};

template<typename T>
struct unique_ptr_less
{
  inline bool operator()(const std::unique_ptr<T>& l,
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
