#ifndef PARACUBER_PRIORITYQUEUELOCKSEMANTICS_HPP
#define PARACUBER_PRIORITYQUEUELOCKSEMANTICS_HPP

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace paracuber {
template<typename T>
class PriorityQueueLockSemantics
{
  public:
  PriorityQueueLockSemantics(size_t size = 1)
    : m_queue(size)
  {}
  ~PriorityQueueLockSemantics() {}

  void pushNoLock(T obj)
  {
    ++m_size;
    m_queue.push_back(std::move(obj));
    std::push_heap(m_queue.begin(), m_queue.end());
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
    std::pop_heap(m_queue.begin(), m_queue.end());
    m_queue.pop_back();
    return result;
  }

  T pop()
  {
    std::unique_lock lock(m_mutex);
    return popNoLock();
  }

  inline bool empty() const { return m_size == 0; }

  inline int64_t size() const { return m_size; }

  inline std::mutex& getMutex() { return m_mutex; }

  private:
  std::vector<T> m_queue;
  std::atomic_int64_t m_size = 0;
  std::mutex m_mutex;
};

template<typename T>
using PriorityQueueLockSemanticsUniquePtr =
  PriorityQueueLockSemantics<std::unique_ptr<T>>;
}

#endif
