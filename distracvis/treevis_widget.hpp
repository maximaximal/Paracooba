#pragma once

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Platform/GLContext.h>

#include <gtkmm/glarea.h>
#include <memory>

namespace parac::distracvis {
class TreeVisWidget : public Gtk::GLArea {
  public:
  explicit TreeVisWidget(Magnum::Platform::GLContext& context);
  virtual ~TreeVisWidget();

  private:
  void onRealize();
  bool onRender(const Glib::RefPtr<Gdk::GLContext>& context);
  void onResize(int width, int height);
  void onUnrealize();

  Magnum::Platform::GLContext& m_context;

  struct Internal;
  std::unique_ptr<Internal> m_internal;
};
}
