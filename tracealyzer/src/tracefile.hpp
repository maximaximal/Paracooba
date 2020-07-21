#ifndef PARACOOBA_TRACEALYZER_TRACEFILE
#define PARACOOBA_TRACEALYZER_TRACEFILE

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include <boost/iostreams/device/mapped_file.hpp>

#include "../../libparacooba/include/paracooba/tracer.hpp"

namespace paracooba::tracealyzer {
class TraceFileView;

// Copied over from util.cpp so there is no link required.
inline std::string
BytePrettyPrint(size_t bytes)
{
  auto base = (double)std::log(bytes) / (double)std::log(1024);
  const char* suffixArr[] = { "B", "kiB", "MiB", "GiB", "TiB", "PiB" };
  return std::to_string(
           (size_t)std::round(std::pow(1024, base - std::floor(base)))) +
         suffixArr[(size_t)std::floor(base)] + " (=" + std::to_string(bytes) +
         " BYTES)";
}

class TraceFile
{
  public:
  // Taken from https://stackoverflow.com/a/31886483
  template<typename Type>
  class Iterator : public std::iterator<std::random_access_iterator_tag, Type>
  {
    public:
    using difference_type =
      typename std::iterator<std::random_access_iterator_tag,
                             Type>::difference_type;

    Iterator()
      : _ptr(nullptr)
    {}
    Iterator(Type* rhs)
      : _ptr(rhs)
    {}
    Iterator(const Iterator& rhs)
      : _ptr(rhs._ptr)
    {}
    /* inline Iterator& operator=(Type* rhs) {_ptr = rhs; return *this;} */
    /* inline Iterator& operator=(const Iterator &rhs) {_ptr = rhs._ptr; return
     * *this;} */
    inline Iterator& operator+=(difference_type rhs)
    {
      _ptr += rhs;
      return *this;
    }
    inline Iterator& operator-=(difference_type rhs)
    {
      _ptr -= rhs;
      return *this;
    }
    inline Type& operator*() const { return *_ptr; }
    inline Type* operator->() const { return _ptr; }
    inline Type& operator[](difference_type rhs) const { return _ptr[rhs]; }

    inline Iterator& operator++()
    {
      ++_ptr;
      return *this;
    }
    inline Iterator& operator--()
    {
      --_ptr;
      return *this;
    }
    inline Iterator operator++(int) const
    {
      Iterator tmp(*this);
      ++_ptr;
      return tmp;
    }
    inline Iterator operator--(int) const
    {
      Iterator tmp(*this);
      --_ptr;
      return tmp;
    }
    /* inline Iterator operator+(const Iterator& rhs) {return
     * Iterator(_ptr+rhs.ptr);} */
    inline difference_type operator-(const Iterator& rhs) const
    {
      return _ptr - rhs._ptr;
    }
    inline Iterator operator+(difference_type rhs) const
    {
      return Iterator(_ptr + rhs);
    }
    inline Iterator operator-(difference_type rhs) const
    {
      return Iterator(_ptr - rhs);
    }
    friend inline Iterator operator+(difference_type lhs, const Iterator& rhs)
    {
      return Iterator(lhs + rhs._ptr);
    }
    friend inline Iterator operator-(difference_type lhs, const Iterator& rhs)
    {
      return Iterator(lhs - rhs._ptr);
    }

    inline bool operator==(const Iterator& rhs) const
    {
      return _ptr == rhs._ptr;
    }
    inline bool operator!=(const Iterator& rhs) const
    {
      return _ptr != rhs._ptr;
    }
    inline bool operator>(const Iterator& rhs) const { return _ptr > rhs._ptr; }
    inline bool operator<(const Iterator& rhs) const { return _ptr < rhs._ptr; }
    inline bool operator>=(const Iterator& rhs) const
    {
      return _ptr >= rhs._ptr;
    }
    inline bool operator<=(const Iterator& rhs) const
    {
      return _ptr <= rhs._ptr;
    }

    private:
    Type* _ptr;
  };
  using TraceEntryIterator = Iterator<TraceEntry>;

  TraceFile(const std::string& path);
  ~TraceFile();

  inline TraceEntry& operator[](size_t index)
  {
    assert(index < entries);
    assert(sink.is_open());
    assert(sink.data());
    return *reinterpret_cast<TraceEntry*>(
      &sink.data()[index * sizeof(TraceEntry)]);
  }
  inline const TraceEntry& operator[](size_t index) const
  {
    assert(index < entries);
    assert(sink.is_open());
    assert(sink.const_data());
    return *reinterpret_cast<const TraceEntry*>(
      &sink.const_data()[index * sizeof(TraceEntry)]);
  }

  void sort();

  inline TraceEntryIterator begin() { return TraceEntryIterator(&(*this)[0]); }
  inline TraceEntryIterator end()
  {
    return TraceEntryIterator(&(*this)[0] + entries);
  }

  inline size_t size() const { return entries; }

  TraceFileView getOnlyTraceKind(traceentry::Kind kind);

  void printUtilizationLog();
  void printTaskRuntimeLog();
  void printNetworkLog();

  void swap(size_t i1, size_t i2);

  size_t forwardSearchForKind(size_t start, traceentry::Kind kind);

  private:
  boost::iostreams::mapped_file sink;
  size_t byteSize = 0;
  size_t entries = 0;

  void causalSort();
  bool causalFixup(size_t i);
};
}

#endif
