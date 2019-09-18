#ifndef PARACUBER_READYWAITER_HPP
#define PARACUBER_READYWAITER_HPP

#include <functional>
#include <queue>

namespace paracuber {
template<typename T>
class ReadyWaiter
{
  public:
  ReadyWaiter() {}
  ~ReadyWaiter() {}

  using ReadyCB = std::function<void(T&)>;

  inline void reset()
  {
    m_ready = false;
    m_value = nullptr;
  }
  inline void setReady(T* value)
  {
    m_ready = true;
    while(!m_readyCBs.empty()) {
      auto cb = m_readyCBs.front();
      m_readyCBs.pop();
      cb(*m_value);
    };
  }
  inline bool isReady() { return m_ready; }
  inline void callWhenReady(ReadyCB cb)
  {
    if(isReady())
      cb(*m_value);
    else
      m_readyCBs.push(cb);
  }

  private:
  volatile bool m_ready = false;
  T* m_value = nullptr;
  std::queue<ReadyCB> m_readyCBs;
};
}

#endif
