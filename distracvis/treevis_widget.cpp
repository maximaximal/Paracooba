#include "treevis_widget.hpp"

#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Trade/MeshData.h>
#include <gdkmm/device.h>
#include <gdkmm/event.h>

#include <gdkmm/screen.h>
#include <iostream>

using namespace Magnum;
using namespace Magnum::Math::Literals;

namespace parac::distracvis {
struct TreeVisWidget::Internal {
  Magnum::GL::Mesh mesh;
  Magnum::Shaders::Phong shader;
  Magnum::Matrix4 transformation, projection;

  bool rotating = false;
  Vector2 lastMousePos;
};

// Following primitives guide from
// https://doc.magnum.graphics/magnum/examples-primitives.html

TreeVisWidget::TreeVisWidget(Platform::GLContext& context)
  : m_context(context) {
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

  add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |
             Gdk::BUTTON_RELEASE_MASK);

  /* Connect signals to their respective handlers */
  signal_realize().connect(sigc::mem_fun(this, &TreeVisWidget::onRealize));
  signal_render().connect(sigc::mem_fun(this, &TreeVisWidget::onRender));
  signal_resize().connect(sigc::mem_fun(this, &TreeVisWidget::onResize));
  signal_unrealize().connect(sigc::mem_fun(this, &TreeVisWidget::onUnrealize));
  signal_motion_notify_event().connect(
    sigc::mem_fun(this, &TreeVisWidget::onMotionEvent));
  signal_button_press_event().connect(
    sigc::mem_fun(this, &TreeVisWidget::onButtonEvent));
  signal_button_release_event().connect(
    sigc::mem_fun(this, &TreeVisWidget::onButtonEvent));
}
TreeVisWidget::~TreeVisWidget() {}

void
TreeVisWidget::onRealize() {
  /* Make sure the OpenGL context is current then configure it */
  make_current();
  m_context.create();

  GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
  GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

  m_internal = std::make_unique<Internal>();

  Trade::MeshData cube = Primitives::cubeSolid();
  GL::Buffer vertices;
  vertices.setData(
    MeshTools::interleave(cube.positions3DAsArray(), cube.normalsAsArray()));

  std::pair<Containers::Array<char>, MeshIndexType> compressed =
    MeshTools::compressIndices(cube.indicesAsArray());
  GL::Buffer indices;
  indices.setData(compressed.first);

  m_internal->mesh.setPrimitive(cube.primitive())
    .setCount(cube.indexCount())
    .addVertexBuffer(std::move(vertices),
                     0,
                     Shaders::Phong::Position{},
                     Shaders::Phong::Normal{})
    .setIndexBuffer(std::move(indices), 0, compressed.second);

  m_internal->transformation =
    Matrix4::rotationX(30.0_degf) * Matrix4::rotationY(40.0_degf);
  m_internal->projection = Matrix4::perspectiveProjection(
                             35.0_degf,
                             Vector2(get_width(), get_height()).aspectRatio(),
                             0.01f,
                             100.0f) *
                           Matrix4::translation(Vector3::zAxis(-10.0f));
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

  auto color = Color3::fromHsv({ 35.0_degf, 1.0f, 1.0f });

  /* TODO: Add your drawing code here */
  m_internal->shader.setLightPositions({ { 7.0f, 5.0f, 2.5f, 0.0f } })
    .setDiffuseColor(color)
    .setAmbientColor(Color3::fromHsv({ color.hue(), 1.0f, 0.3f }))
    .setTransformationMatrix(m_internal->transformation)
    .setNormalMatrix(m_internal->transformation.normalMatrix())
    .setProjectionMatrix(m_internal->projection)
    .draw(m_internal->mesh);

  /* Clean up Magnum state and back to Gtkmm */
  GL::Context::current().resetState(GL::Context::State::EnterExternal);
  return true;
}

void
TreeVisWidget::onResize(int width, int height) {
  m_internal->projection =
    Matrix4::perspectiveProjection(
      35.0_degf, Vector2(width, height).aspectRatio(), 0.01f, 100.0f) *
    Matrix4::translation(Vector3::zAxis(-10.0f));
}

void
TreeVisWidget::onUnrealize() {
  /* TODO: Add your clean-up code here */
}

bool
TreeVisWidget::onButtonEvent(GdkEventButton* e) {
  if(e->button == 3) {
    switch(e->type) {
      case GDK_BUTTON_PRESS:
        m_internal->lastMousePos.x() = e->x;
        m_internal->lastMousePos.y() = e->y;
        m_internal->rotating = true;
        return true;
      case GDK_BUTTON_RELEASE:
        handleMouseRotation(Vector2(e->x, e->y) - m_internal->lastMousePos);
        m_internal->rotating = false;
        return true;
      default:
        break;
    }
  }
  return false;
}

bool
TreeVisWidget::onMotionEvent(GdkEventMotion* e) {
  if(m_internal->rotating) {
    handleMouseRotation(Vector2(e->x, e->y) - m_internal->lastMousePos);
    m_internal->lastMousePos.x() = e->x;
    m_internal->lastMousePos.y() = e->y;
    return true;
  }
  return false;
}

void
TreeVisWidget::handleMouseRotation(Vector2 delta) {
  delta /= get_screen()->get_resolution();

  m_internal->transformation = Matrix4::rotationX(Rad{ delta.y() }) *
                               m_internal->transformation *
                               Matrix4::rotationY(Rad{ delta.x() });
  queue_draw();
}
}
