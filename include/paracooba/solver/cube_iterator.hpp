#ifndef PARACOOBA_SOLVER_CUBEITERATOR_HPP
#define PARACOOBA_SOLVER_CUBEITERATOR_HPP

#include "types.hpp"

namespace parac::solver {
class CubeIterator {
  public:
  typedef CubeIterator self_type;
  typedef Literal value_type;
  typedef const value_type& reference;
  typedef const value_type* pointer;
  typedef std::forward_iterator_tag iterator_category;
  typedef size_t difference_type;

  CubeIterator() = default;

  CubeIterator(const pointer& pos)
    : m_pos(pos) {}
  CubeIterator(Cube::iterator it)
    : m_pos(&*it) {}
  CubeIterator(const Cube::const_iterator& it)
    : m_pos(&*it) {}

  // PREFIX
  self_type operator++() {
    ++m_pos;
    return *this;
  }

  // POSTFIX
  self_type operator++(int junk) {
    (void)junk;
    self_type self = *this;
    ++(*this);
    return self;
  }

  size_t operator-(const CubeIterator& o) const { return m_pos - o.m_pos; }

  reference operator*() { return *m_pos; }
  pointer operator->() { return &(**this); }

  CubeIterator operator+(size_t i) {
    self_type it = *this;
    for(size_t j = 0; j < i; ++j) {
      ++it;
    }
    return it;
  }

  bool operator==(const self_type& rhs) const { return m_pos == rhs.m_pos; }
  bool operator!=(const self_type& rhs) const { return !(*this == rhs); }

  private:
  pointer m_pos = nullptr;
};
struct CubeIteratorRange {
  CubeIteratorRange() = default;
  CubeIteratorRange(const CubeIterator& begin, const CubeIterator& end)
    : m_begin(begin)
    , m_end(end) {}
  ~CubeIteratorRange() = default;

  const CubeIterator& begin() const { return m_begin; }
  const CubeIterator& end() const { return m_end; }
  size_t size() const { return m_end - m_begin; }

  const CubeIterator m_begin = nullptr;
  const CubeIterator m_end = nullptr;

  operator bool() { return m_begin != m_end && m_begin != nullptr; }

  bool contains0() {
    if(!*this)
      return false;
    for(auto l : *this) {
      if(l == 0)
        return true;
    }
    return false;
  }
};
}

#endif
