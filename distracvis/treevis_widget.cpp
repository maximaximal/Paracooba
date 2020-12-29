#include "treevis_widget.hpp"

using namespace Magnum;

namespace parac::distracvis {
TreeVisWidget::TreeVisWidget(Platform::GLContext& context)
  : _context(context) {
  /* Automatically re-render everything every time it needs to be drawn */
  set_auto_render();

  /* Set size requests and scaling behavior */
  set_hexpand();
  set_vexpand();
  set_halign(Gtk::ALIGN_FILL);
  set_valign(Gtk::ALIGN_FILL);
  set_size_request(800, 600);

  /* Set desired OpenGL version */
  set_required_version(4, 5);

  /* Connect signals to their respective handlers */
  signal_realize().connect(sigc::mem_fun(this, &TreeVisWidget::onRealize));
  signal_render().connect(sigc::mem_fun(this, &TreeVisWidget::onRender));
  signal_resize().connect(sigc::mem_fun(this, &TreeVisWidget::onResize));
  signal_unrealize().connect(sigc::mem_fun(this, &TreeVisWidget::onUnrealize));
}
TreeVisWidget::~TreeVisWidget() {}

void
TreeVisWidget::onRealize() {
  /* Make sure the OpenGL context is current then configure it */
  make_current();
  _context.create();

  /* TODO: Add your initialization code here */
}

bool
TreeVisWidget::onRender(const Glib::RefPtr<Gdk::GLContext>& context) {
  /* Reset state to avoid Gtkmm affecting Magnum */
  GL::Context::current().resetState(GL::Context::State::ExitExternal);

  /* Retrieve the ID of the relevant framebuffer */
  GLint framebufferID;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebufferID);

  /* Attach Magnum's framebuffer manager to the framebuffer provided by Gtkmm */
  auto gtkmmDefaultFramebuffer =
    GL::Framebuffer::wrap(framebufferID, { {}, { get_width(), get_height() } });

  /* Clear the frame */
  gtkmmDefaultFramebuffer.clear(GL::FramebufferClear::Color);

  /* TODO: Add your drawing code here */

  /* Clean up Magnum state and back to Gtkmm */
  GL::Context::current().resetState(GL::Context::State::EnterExternal);
  return true;
}

void
TreeVisWidget::onResize(int width, int height) {
  /* TODO: Add your window-resize handling code here */
}

void
TreeVisWidget::onUnrealize() {
  /* TODO: Add your clean-up code here */
}
}
