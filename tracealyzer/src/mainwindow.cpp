#include "mainwindow.hpp"
#include "tracefile.hpp"

namespace paracooba::tracealyzer {
MainWindow::MainWindow(TraceFile& traceFile)
  : m_traceFile(traceFile)
{
  set_default_size(1000, 800);

}
MainWindow::~MainWindow() {}

}
