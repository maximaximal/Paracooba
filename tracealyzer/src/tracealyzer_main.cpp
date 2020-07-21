#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#ifdef ENABLE_INTERACTIVE_VIEWER
#include <gtkmm-3.0/gtkmm/application.h>
#endif

#include "mainwindow.hpp"
#include "tracefile.hpp"
#include "tracefileview.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using namespace paracooba;
using namespace paracooba::tracealyzer;

int
main(int argc, char* argv[])
{
  bool force_re_sort = false;
  bool generate_utilization_data = false;
  bool generate_taskruntime_data = false;
  bool generate_network_data = false;

  po::options_description desc("Allowed options");
  po::positional_options_description posDesc;
  // clang-format off
  desc.add_options()
    ("help", "produce help message")
    ("force-sort", po::bool_switch(&force_re_sort)->default_value(false), "force re-sorting the events")
    ("generate-utilization-data", po::bool_switch(&generate_utilization_data)->default_value(false), "print worker utilization data and exit")
    ("generate-taskruntime-data", po::bool_switch(&generate_taskruntime_data)->default_value(false), "print task runtime data (unsorted!) in format Runtime Path and exit")
    ("generate-network-data", po::bool_switch(&generate_network_data)->default_value(false), "print network usage data and exit")
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

  if(force_re_sort) {
    traceFile.sort();
  }

  if(generate_utilization_data) {
    traceFile.printUtilizationLog();
    return EXIT_SUCCESS;
  }

  if(generate_taskruntime_data) {
    traceFile.printTaskRuntimeLog();
    return EXIT_SUCCESS;
  }

  if(generate_network_data) {
    traceFile.printNetworkLog();
    return EXIT_SUCCESS;
  }

#ifdef ENABLE_INTERACTIVE_VIEWER
  auto app = Gtk::Application::create("at.jku.fmv.paracooba.tracealyzer");

  MainWindow mainWindow(traceFile);

  return app->run(mainWindow);
#else
  cerr << "!! Interactive viewer not available in this build! Specify offline "
          "analysis method instead."
       << endl;
#endif
}
