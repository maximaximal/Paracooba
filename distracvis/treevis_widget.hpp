#pragma once

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Platform/GLContext.h>

#include <gtkmm/glarea.h>
#include <memory>

namespace distrac {
class tracefile;
}

namespace parac::distracvis {
class TreeVisWidget : public Gtk::GLArea {
  public:
  explicit TreeVisWidget(distrac::tracefile& tracefile,
                         Magnum::Platform::GLContext& context);
  virtual ~TreeVisWidget() noexcept override;

  void queueUpdateShownTimespan(int64_t passedMs);

  private:
  distrac::tracefile& m_tracefile;
  int64_t m_passedMs = 0;
  bool m_queueUpdateShownTimespan = false;

  int m_z = -100;

  void updateShownTimespan();

  void onRealize();
  bool onRender(const Glib::RefPtr<Gdk::GLContext>& context);
  void onResize(int width, int height);
  void onUnrealize();

  bool onButtonEvent(GdkEventButton*);
  bool onMotionEvent(GdkEventMotion*);
  bool onScrollEvent(GdkEventScroll*);

  void handleMouseRotation(Magnum::Vector2 mouseDelta);

  Magnum::Platform::GLContext& m_context;

  struct Internal;
  std::unique_ptr<Internal> m_internal;
};
}
