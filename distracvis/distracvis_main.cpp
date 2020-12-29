#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Platform/GLContext.h>
#include <cstdlib>
#include <gtkmm/application.h>

#include "mainwindow.hpp"

#include <iostream>

using namespace Magnum;
using namespace parac::distracvis;

int
main(int argc, char** argv) {
  if(argc != 2) {
    std::cerr << "Give a trace file to open!" << std::endl;
    return EXIT_FAILURE;
  }

  argc = 1;
  Platform::GLContext context{ NoCreate, argc, argv };
  auto app = Gtk::Application::create(argc, argv, "at.maxheisinger.distracvis");

  MainWindow window{ context, argv[1] };
  window.show();

  /* Hand over control to Gtk */
  return app->run(window);
}
