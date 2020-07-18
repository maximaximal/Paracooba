#ifndef PARACOOBA_TRACEALYZER_TRACEFILEVIEW
#define PARACOOBA_TRACEALYZER_TRACEFILEVIEW

#include <iterator>

#include "tracefile.hpp"

#include "../../libparacooba/include/paracooba/tracer.hpp"

namespace paracooba::tracealyzer {
class TraceFileView
{
  public:
  typedef bool (*Predicate)(TraceEntry&, void*, uint64_t);

  TraceFileView(TraceFile& file,
                size_t index,
                Predicate predicate,
                void* userdata1,
                uint64_t userdata2)
    : file(file)
    , index(index)
    , predicate(predicate)
    , userdata1(userdata1)
    , userdata2(userdata2)
  {}
  TraceFileView operator++()
  {
    while(index < file.size() - 1 &&
          !predicate(file[++index], userdata1, userdata2)) {
    }
    return *this;
  }
  TraceFileView operator++(int junk)
  {
    while(index < file.size() - 1 &&
          !predicate(file[++index], userdata1, userdata2)) {
    }
    return *this;
  }
  TraceEntry& operator*() { return file[index]; }
  TraceEntry* operator->() { return &file[index]; }
  bool operator==(const TraceFileView& rhs) { return index == rhs.index; }
  bool operator!=(const TraceFileView& rhs) { return index != rhs.index; }
  bool operator==(const TraceFile::TraceEntryIterator& rhs)
  {
    return &(*(*this)) == &(*rhs);
  }
  bool operator!=(const TraceFile::TraceEntryIterator& rhs)
  {
    return &(*(*this)) != &(*rhs);
  }

  bool hasNext() { return index < file.size() - 1; }

  private:
  TraceFile& file;
  size_t index;
  Predicate predicate;
  void* userdata1;
  uint64_t userdata2;
};
}

#endif
