#pragma once

#include "treevis_widget.hpp"
#include <gtkmm/box.h>
#include <gtkmm/notebook.h>
#include <string_view>

#include <gtkmm/adjustment.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/scale.h>

#include <distrac/analysis/tracefile.hpp>

namespace Magnum::Platform {
class GLContext;
}

namespace parac::distracvis {
class MainWindow : public Gtk::ApplicationWindow {
  public:
  MainWindow(Magnum::Platform::GLContext& context, const std::string& file);
  virtual ~MainWindow();

  private:
  Magnum::Platform::GLContext& m_glContext;
  std::string m_file;

  void timeSliderValueChanged();

  distrac::tracefile m_tracefile;

  TreeVisWidget m_treeVisWidget;

  Gtk::Box m_box_root;
  Gtk::Box m_box_treeVisAndSlider;
  Gtk::Notebook m_sidebar;

  Glib::RefPtr<Gtk::Adjustment> m_timeSliderAdjustment;
  Gtk::Scale m_timeSlider;
};
}
