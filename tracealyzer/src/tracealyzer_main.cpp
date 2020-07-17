#include <cstdlib>
#include <iostream>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#include <boost/iostreams/device/mapped_file.hpp>
#include <stdexcept>

#include "../../libparacooba/include/paracooba/tracer.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using namespace paracooba;

// Copied over from util.cpp so there is no link required.
std::string
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

  TraceFile(const std::string& path)
  {
    sink.open(path);
    if(!sink.is_open()) {
      cerr << "!! Could not open file as memory mapped file!" << endl;
      exit(EXIT_FAILURE);
    }

    byteSize = sink.size();
    entries = byteSize / sizeof(TraceEntry);

    clog << "-> Opened trace of size " << BytePrettyPrint(byteSize)
         << ", containing " << entries << " trace entries." << endl;

    if(size() > 0) {
      TraceEntry& first = (*this)[0];
      if(first.kind != traceentry::Kind::ClientBegin ||
         !first.body.clientBegin.sorted) {
        clog << "Begin sorting... ";
        sort();
        clog << " sorting finished!" << endl;
      }
    }
  }
  ~TraceFile() {}

  TraceEntry& operator[](size_t index)
  {
    assert(index < entries);
    assert(sink.is_open());
    assert(sink.data());
    return *reinterpret_cast<TraceEntry*>(
      &sink.data()[index * sizeof(TraceEntry)]);
  }
  const TraceEntry& operator[](size_t index) const
  {
    assert(index < entries);
    assert(sink.is_open());
    assert(sink.const_data());
    return *reinterpret_cast<const TraceEntry*>(
      &sink.const_data()[index * sizeof(TraceEntry)]);
  }
  void sort() { std::sort(begin(), end()); }

  TraceEntryIterator begin() { return TraceEntryIterator(&(*this)[0]); }
  TraceEntryIterator end() { return TraceEntryIterator(&(*this)[0] + entries); }

  inline size_t size() const { return entries; }

  private:
  boost::iostreams::mapped_file sink;
  size_t byteSize = 0;
  size_t entries = 0;
};

int
main(int argc, char* argv[])
{
  po::options_description desc("Allowed options");
  po::positional_options_description posDesc;
  // clang-format off
  desc.add_options()
    ("help", "produce help message")
    ("trace", po::value<std::string>(), "concatenated trace file")
  ;
  posDesc.add("trace", -1);
  // clang-format on
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
              .options(desc)
              .positional(posDesc)
              .allow_unregistered()
              .run(),
            vm);
  po::notify(vm);

  if(vm.count("help")) {
    cout << "This tool helps analyzing traces of the parac SAT solver." << endl;
    cout << "First, concatenate all logs into one file using cat." << endl;
    cout << "A script to help with that task is provided in "
            "scripts/concatenate_traces.sh"
         << endl;
    cout << "Then, the file can be sorted and analyzed." << endl << endl;
    cout << desc << endl;
    return EXIT_SUCCESS;
  }

  if(!vm.count("trace")) {
    cerr << "!! Requires a trace file!" << endl;
    return EXIT_FAILURE;
  }

  std::string trace = vm["trace"].as<std::string>();
  if(!fs::exists(trace)) {
    cerr << "!! Trace file \"" << trace << "\" does not exist!" << endl;
    return EXIT_FAILURE;
  }
  if(fs::is_directory(trace)) {
    cerr << "!! File \"" << trace << "\" no file, but a directory!" << endl;
    return EXIT_FAILURE;
  }

  TraceFile traceFile(trace);

  for(auto& e : traceFile) {
    cout << e << endl;
  }

  return EXIT_SUCCESS;
}
