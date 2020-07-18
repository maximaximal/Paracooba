#ifndef PARACOOBA_TRACEALYZER_MAINWINDOW
#define PARACOOBA_TRACEALYZER_MAINWINDOW

#include <gtkmm-3.0/gtkmm/window.h>

namespace paracooba::tracealyzer {
class TraceFile;

class MainWindow : public Gtk::Window
{
  public:
  MainWindow(TraceFile& traceFile);
  virtual ~MainWindow();

  private:
  TraceFile& m_traceFile;
};
}

#endif
