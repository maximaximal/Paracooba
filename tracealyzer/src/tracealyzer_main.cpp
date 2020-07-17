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

  cout << traceFile[0] << endl;

  return EXIT_SUCCESS;
}
