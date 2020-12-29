#include "mainwindow.hpp"
#include <gtkmm/enums.h>

#include <iostream>

namespace parac::distracvis {
MainWindow::MainWindow(Magnum::Platform::GLContext& context,
                       const std::string& file)
  : m_glContext(context)
  , m_file(file)
  , m_tracefile(file)
  , m_treeVisWidget(context)
  , m_box_treeVisAndSlider(Gtk::Orientation::ORIENTATION_VERTICAL)
  , m_timeSliderAdjustment(
      Gtk::Adjustment::create(0, 0, m_tracefile.trace_duration_ms(), 10, 0))
  , m_timeSlider(m_timeSliderAdjustment,
                 Gtk::Orientation::ORIENTATION_HORIZONTAL) {
  m_box_treeVisAndSlider.add(m_treeVisWidget);
  m_box_treeVisAndSlider.add(m_timeSlider);

  m_box_root.add(m_box_treeVisAndSlider);
  m_box_root.add(m_sidebar);

  m_box_root.show_all();

  add(m_box_root);

  set_size_request(1000, 600);

  m_sidebar.set_size_request(200, 600);
  m_sidebar.set_vexpand();

  std::cout << "Duration MS: " << m_tracefile.trace_duration_ms() << std::endl;
}
MainWindow::~MainWindow() {}
}
