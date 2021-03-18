#include "treevis_widget.hpp"
#include "distrac/analysis/event_definition.hpp"
#include "distrac/analysis/event_iterator.hpp"
#include "distrac/analysis/property.hpp"
#include "distrac/analysis/property_definition.hpp"
#include "distrac/analysis/tracefile.hpp"

#include <Corrade/Containers/ArrayViewStl.h>

#include <Magnum/GL/AbstractFramebuffer.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Shaders/Flat.h>
#include <Magnum/Trade/MeshData.h>
#include <boost/pool/pool_alloc.hpp>
#include <gdk/gdkkeysyms.h>
#include <gdkmm/cursor.h>
#include <gdkmm/device.h>
#include <gdkmm/event.h>

#include <limits>
#include <paracooba/common/path.h>
#include <paracooba/common/status.h>

#include <cstdint>
#include <gdkmm/screen.h>
#include <iostream>

using namespace Magnum;
using namespace Magnum::Math::Literals;

namespace parac::distracvis {
Matrix4
getTransformationFromPathAndNodeId(size_t nodeId, parac_path path) {
  Vector3 t(0, 0, 0);

  float offset = 1000;

  t.y() = nodeId * 10;
  t.y() -= 1000;

  if(path.rep == PARAC_PATH_PARSER) {
    return Matrix4().scaling(Vector3(3, 3, 3)).translation(t);
  } else if(parac_path_is_root(path)) {
    return Matrix4().translation(t).scaling(Vector3(2, 2, 2)).translation(t);
  } else {
    for(size_t i = 0; i < parac_path_length(path); ++i) {
      if(parac_path_get_assignment(path, i + 1)) {
        t.x() -= offset;
      } else {
        t.z() -= offset;
      }
      offset /= 2;
    }
  }

  return Matrix4().translation(t);
}

struct TaskInstance {
  explicit TaskInstance(size_t nodeId,
                        parac_path path,
                        parac_status state = PARAC_UNKNOWN)
    : transformation(getTransformationFromPathAndNodeId(nodeId, path)) {
    (void)state;
    if(state == PARAC_UNKNOWN) {
      color.b() = 1;
    }
    if(state == PARAC_PENDING) {
      color.b() = 1;
      color.r() = 1;
    }
    if(state == PARAC_SAT) {
      color.g() = 1;
    }
    if(state == PARAC_UNSAT) {
      color.r() = 1;
    }
  }

  Matrix4 transformation;
  Color3 color = Color3(0, 0, 0);
};

struct TreeVisWidget::Internal {
  Magnum::GL::Mesh mesh;
  Magnum::Shaders::Flat3D shader =
    Shaders::Flat3D(Shaders::Flat3D::Flag::InstancedTransformation);
  Magnum::Matrix4 transformation, projection;

  bool rotating = false;
  Vector2 lastMousePos;

  std::vector<TaskInstance, boost::pool_allocator<TaskInstance>> tasks;

  int64_t lastPassedMSTarget = std::numeric_limits<int64_t>::max();
};

// Following primitives guide from
// https://doc.magnum.graphics/magnum/examples-primitives.html

TreeVisWidget::TreeVisWidget(distrac::tracefile& tracefile,
                             Platform::GLContext& context)
  : m_tracefile(tracefile)
  , m_context(context) {
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
             Gdk::BUTTON_RELEASE_MASK | Gdk::SCROLL_MASK);

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
  signal_scroll_event().connect(
    sigc::mem_fun(this, &TreeVisWidget::onScrollEvent), false);
}
TreeVisWidget::~TreeVisWidget() noexcept {}

void
TreeVisWidget::queueUpdateShownTimespan(int64_t passedMs) {
  m_passedMs = passedMs;
  m_queueUpdateShownTimespan = true;

  queue_draw();
}

void
TreeVisWidget::updateShownTimespan() {
  auto start_processing_task =
    m_tracefile.get_event_id("start_processing_task");
  auto finish_processing_task =
    m_tracefile.get_event_id("finish_processing_task");

  assert(start_processing_task >= 0);
  assert(finish_processing_task >= 0);

  auto start_processing_task_prop_path =
    m_tracefile.event_definitions()[start_processing_task].get_property_id(
      "path");

  auto finish_processing_task_prop_path =
    m_tracefile.event_definitions()[finish_processing_task].get_property_id(
      "path");
  auto finish_processing_task_prop_result =
    m_tracefile.event_definitions()[finish_processing_task].get_property_id(
      "result");

  auto filtered =
    m_tracefile.filtered({ static_cast<uint8_t>(start_processing_task),
                           static_cast<uint8_t>(finish_processing_task) });

  auto begin = filtered.begin();

  bool first = true;
  int64_t beginTime = 0;

  if(m_internal->lastPassedMSTarget < m_passedMs) {
    for(; begin != filtered.end(); ++begin) {
      auto& it = *begin;

      if(first) {
        beginTime = it.timestamp_with_offset();
        first = false;
      }
      if(it.timestamp_with_offset() - beginTime >
         m_internal->lastPassedMSTarget * 1000000) {
        break;
      }
    }
  } else {
    m_internal->tasks.clear();
  }

  m_internal->lastPassedMSTarget = m_passedMs;

  for(; begin != filtered.end(); ++begin) {
    auto& it = *begin;

    if(first) {
      beginTime = it.timestamp_with_offset();
      first = false;
    }
    if(it.timestamp_with_offset() - beginTime > m_passedMs * 1000000) {
      break;
    }

    if(it.id() == start_processing_task) {
      parac_path p =
        it.property(start_processing_task_prop_path).as<parac_path>();

      if(parac_path_is_overlength(p))
        continue;

      m_internal->tasks.emplace_back(it.node().tracefile_location_index(), p);
    } else if(it.id() == finish_processing_task) {
      parac_path p =
        it.property(finish_processing_task_prop_path).as<parac_path>();

      if(parac_path_is_overlength(p))
        continue;

      m_internal->tasks.emplace_back(
        it.node().tracefile_location_index(),
        p,
        static_cast<parac_status>(
          it.property(finish_processing_task_prop_result).as<uint16_t>()));
    }
  }

  m_internal->mesh.setInstanceCount(m_internal->tasks.size())
    .addVertexBufferInstanced(GL::Buffer(m_internal->tasks),
                              1,
                              0,
                              Shaders::Flat3D::TransformationMatrix{},
                              Shaders::Flat3D::Color3{});

  queue_draw();
}

void
TreeVisWidget::onRealize() {
  /* Make sure the OpenGL context is current then configure it */
  make_current();
  m_context.create();

  GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
  GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
  GL::Renderer::setDepthFunction(GL::Renderer::DepthFunction::Greater);

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
                     Shaders::Flat3D::Position{},
                     Shaders::Flat3D::Color3{})
    .setIndexBuffer(std::move(indices), 0, compressed.second);

  m_internal->mesh.setInstanceCount(m_internal->tasks.size())
    .addVertexBufferInstanced(GL::Buffer(m_internal->tasks),
                              1,
                              0,
                              Shaders::Flat3D::TransformationMatrix{});

  onResize(get_width(), get_height());
}

bool
TreeVisWidget::onRender(const Glib::RefPtr<Gdk::GLContext>& context) {
  (void)context;

  /* Reset state to avoid Gtkmm affecting Magnum */
  GL::Context::current().resetState(GL::Context::State::ExitExternal);

  /* Retrieve the ID of the relevant framebuffer */
  GLint framebufferID;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebufferID);

  /* Attach Magnum's framebuffer manager to the framebuffer provided by Gtkmm */
  auto gtkmmDefaultFramebuffer =
    GL::Framebuffer::wrap(framebufferID, { {}, { get_width(), get_height() } });

  /* Clear the frame */
  gtkmmDefaultFramebuffer.clear(GL::FramebufferClear::Color |
                                GL::FramebufferClear::Depth);

  auto color = Color3::fromHsv({ 35.0_degf, 1.0f, 1.0f });

  if(m_queueUpdateShownTimespan) {
    updateShownTimespan();
    m_queueUpdateShownTimespan = false;
  }

  m_internal->shader.setColor(color)
    .setTransformationProjectionMatrix(m_internal->projection *
                                       m_internal->transformation)
    .draw(m_internal->mesh);

  /* Clean up Magnum state and back to Gtkmm */
  GL::Context::current().resetState(GL::Context::State::EnterExternal);
  return true;
}

void
TreeVisWidget::onResize(int width, int height) {
  m_internal->projection =
    Matrix4::perspectiveProjection(
      35.0_degf, Vector2(width, height).aspectRatio(), 0.01f, 20000.0f) *
    Matrix4::translation(Vector3::zAxis(m_z));

  queue_draw();
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

bool
TreeVisWidget::onScrollEvent(GdkEventScroll* scroll) {
  if(scroll->direction == GDK_SCROLL_UP) {
    m_z += 100;
    onResize(get_width(), get_height());
    return true;
  }
  if(scroll->direction == GDK_SCROLL_DOWN) {
    m_z -= 100;
    onResize(get_width(), get_height());
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
