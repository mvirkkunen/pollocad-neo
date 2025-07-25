#include <atomic>

#include "occtview.h"
#include "backgroundexecutor.h"

#include <QRunnable>

#include <AIS_AnimationCamera.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <AIS_ViewCube.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_NeutralWindow.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <OpenGl_ArbDbg.hxx>
#include <OpenGl_Context.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <OpenGl_View.hxx>
#include <OpenGl_Window.hxx>
#include <Prs3d_DatumAspect.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

class ShapeOwner : public Standard_Transient {
    DEFINE_STANDARD_RTTI_INLINE(ShapeOwner, Standard_Transient)

public:
    ShapeOwner() { }
    virtual ~ShapeOwner() { }

    bool isHighlight = false;
    bool isHovered = false;
    bool willUnhover = false;
    QList<SpanObj> spans;
};

class OcctRenderer : public QObject, public AIS_ViewController {
    Q_OBJECT

public:
    ~OcctRenderer() = default;

    void setParent(QQuickItem *parent) { m_parent = parent; }
    void setResult(BackgroundExecutorResult *result);

    void setHoveredPosition(int position);
    void setShowHighlightedShapes(bool show);

    void wheelEvent(int delta);
    void mouseEvent(QPointF pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

    void updateView();
    void updateHoveredSpans();

signals:
    void hoveredSpansChanged(QList<SpanObj> spans);

public slots:
    void init();
    void paint();

private:
    QQuickItem *m_parent = nullptr;
    Handle(OpenGl_GraphicDriver) m_driver;
    Handle(OpenGl_FrameBuffer) m_fbo;
    Handle(Aspect_NeutralWindow) m_window;
    Handle(V3d_View) m_view;
    Handle(V3d_Viewer) m_viewer;
    Handle(AIS_InteractiveContext) m_interactiveContext;
    Handle(AIS_ViewCube) m_viewCube;
    std::vector<Handle(AIS_Shape)> m_shapes;
    bool m_showHighlightedShapes = true;
};

OcctView::OcctView()
{
    connect(this, &QQuickItem::windowChanged, this, &OcctView::handleWindowChanged);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::AllButtons);
}

void OcctView::setResult(BackgroundExecutorResult *result) {
    scheduleRenderJob([this, result] { if (m_renderer) { m_renderer->setResult(result); } });
}

void OcctView::setHoveredPosition(int position) {
    if (position != m_hoveredPosition) {
        m_hoveredPosition = position;
        emit hoveredPositionChanged();
        scheduleRenderJob([this, position] { if (m_renderer) { m_renderer->setHoveredPosition(position); } });
    }
}

void OcctView::setShowHighlightedShapes(bool show) {
    if (show != m_showHighlightedShapes) {
        m_showHighlightedShapes = show;
        emit showHighlightedShapesChanged();
        scheduleRenderJob([show, this]() { m_renderer->setShowHighlightedShapes(show); });
    }
}   

void OcctView::mousePressEvent(QMouseEvent *ev)
{
    handleMouseEvent(ev);
}

void OcctView::mouseReleaseEvent(QMouseEvent *ev)
{
    handleMouseEvent(ev);
}

void OcctView::mouseMoveEvent(QMouseEvent *ev)
{
    handleMouseEvent(ev);
}

void OcctView::hoverMoveEvent(QHoverEvent *ev)
{
    handleMouseEvent(ev);
}

void OcctView::wheelEvent(QWheelEvent *ev)
{
    ev->accept();
    int delta = ev->pixelDelta().y() / 3;
    scheduleRenderJob([=, this] { if (m_renderer) { m_renderer->wheelEvent(delta); } });
}

void OcctView::handleMouseEvent(QSinglePointEvent *ev)
{
    ev->accept();
    auto pos = ev->position();
    auto buttons = ev->buttons();
    auto modifiers = ev->modifiers();
    scheduleRenderJob([=, this] { if (m_renderer) { m_renderer->mouseEvent(pos, buttons, modifiers); } });
}

void OcctView::handleWindowChanged(QQuickWindow *win)
{
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &OcctView::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &OcctView::cleanup, Qt::DirectConnection);
    }
}

void OcctView::sync() {
    if (!m_renderer) {
        m_renderer = new OcctRenderer();
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &OcctRenderer::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &OcctRenderer::paint, Qt::DirectConnection);
        connect(m_renderer, &OcctRenderer::hoveredSpansChanged, this, [this](QList<SpanObj> hoveredSpans) {
            if (hoveredSpans != m_hoveredSpans) {
                m_hoveredSpans = hoveredSpans;
                emit hoveredSpansChanged();
            }
        }, Qt::QueuedConnection);
    }

    m_renderer->setParent(this);
}

void OcctView::cleanup()
{
    delete m_renderer;
    m_renderer = nullptr;
}

void OcctView::scheduleRenderJob(std::function<void()> job) {
    window()->scheduleRenderJob(QRunnable::create(job), QQuickWindow::BeforeRenderingStage);
    window()->update();
}

void OcctView::releaseResources()
{
    window()->scheduleRenderJob(QRunnable::create([=, this] { delete m_renderer; }), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

void OcctRenderer::mouseEvent(QPointF pos, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
{
    Graphic3d_Vec2i posv{static_cast<int>(pos.x()), static_cast<int>(pos.y())};

    Aspect_VKeyMouse vkeymouse = Aspect_VKeyMouse_NONE;
    if (buttons & Qt::LeftButton) vkeymouse |= Aspect_VKeyMouse_LeftButton;
    if (buttons & Qt::MiddleButton) vkeymouse |= Aspect_VKeyMouse_MiddleButton;
    if (buttons & Qt::RightButton) vkeymouse |= Aspect_VKeyMouse_RightButton;

    Aspect_VKeyFlags vkeyflags = Aspect_VKeyFlags_NONE;
    if (modifiers & Qt::ShiftModifier) vkeyflags |= Aspect_VKeyFlags_SHIFT;
    if (modifiers & Qt::ControlModifier) vkeyflags |= Aspect_VKeyFlags_CTRL;
    if (modifiers & Qt::AltModifier) vkeyflags |= Aspect_VKeyFlags_ALT;

    UpdateMousePosition(posv, vkeymouse, vkeyflags, false);
    UpdateMouseButtons(posv, vkeymouse, vkeyflags, false);

    for (const auto &sh : m_shapes) {
        auto owner = Handle(ShapeOwner)::DownCast(sh->GetOwner());
        owner->willUnhover = owner->isHovered;
    }

    for (m_interactiveContext->InitDetected(); m_interactiveContext->MoreDetected(); m_interactiveContext->NextDetected()) {
        if (auto aisObject = Handle(AIS_InteractiveObject)::DownCast(m_interactiveContext->DetectedCurrentOwner()->Selectable())) {
            if (auto owner = Handle(ShapeOwner)::DownCast(aisObject->GetOwner())) {
                owner->willUnhover = false;

                if (!owner->isHovered) {
                    owner->isHovered = true;
                }
            }
        }
    }

    for (const auto &sh : m_shapes) {
        auto owner = Handle(ShapeOwner)::DownCast(sh->GetOwner());
        if (owner->willUnhover) {
            owner->isHovered = false;
        }
    }

    updateHoveredSpans();
}

void OcctRenderer::wheelEvent(int delta)
{
    UpdateZoom(delta);
}

void OcctRenderer::init() {
    if (m_driver) {
        return;
    }

    // N.b. this needs a slightly modified OCCT which does not try to mess with GL stuff it's not supposed to touch

    m_driver = new OpenGl_GraphicDriver(nullptr, false);
    m_driver->ChangeOptions().buffersNoSwap = true;
    m_driver->ChangeOptions().buffersOpaqueAlpha = false;
    m_driver->ChangeOptions().useSystemBuffer = false;
    if (!m_driver->InitContext()) {
        throw std::logic_error{"driver->InitContext failed\n"};
    }

    m_viewer = new V3d_Viewer(m_driver);
    m_viewer->SetDefaultBackgroundColor(Quantity_NOC_GRAY50);
    m_viewer->SetDefaultLights();
    m_viewer->SetLightOn();
    m_viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);

    m_interactiveContext = new AIS_InteractiveContext(m_viewer);

    m_view = m_viewer->CreateView();
    m_view->SetImmediateUpdate(false);

    m_window = new Aspect_NeutralWindow;
    m_window->SetVirtual(true);
    m_window->SetSize(1, 1);

    m_view->SetWindow(m_window, nullptr);

    auto glContext = m_driver->GetSharedContext();

    m_fbo = new OpenGl_FrameBuffer;
    m_fbo->Init(glContext, Graphic3d_Vec2i{1, 1}, GL_RGBA8, GL_DEPTH24_STENCIL8, 0);
    glContext->SetDefaultFrameBuffer(m_fbo);

    m_viewCube = new AIS_ViewCube;
    m_viewCube->SetSize(100.0);
    m_viewCube->SetViewAnimation(ViewAnimation());
    m_viewCube->SetDuration(0.2);
    m_viewCube->SetFixedAnimationLoop(false);
    m_viewCube->SetAutoStartAnimation(true);
    m_viewCube->TransformPersistence()->SetOffset2d(Graphic3d_Vec2i(150, 150));

    Handle(Prs3d_DatumAspect) aspect = new Prs3d_DatumAspect;

    struct { Prs3d_DatumParts part; Quantity_Color color; } axis[] = {
        { Prs3d_DatumParts_XAxis, Quantity_NOC_RED },
        { Prs3d_DatumParts_YAxis, Quantity_NOC_GREEN },
        { Prs3d_DatumParts_ZAxis, Quantity_NOC_BLUE },
    };
    Graphic3d_MaterialAspect mat;
    for (auto &a : axis) {
        aspect->TextAspect(a.part)->SetColor(a.color);
        aspect->ShadingAspect(a.part)->SetAspect(new Graphic3d_AspectFillArea3d{
            Aspect_IS_SOLID, a.color,
            Quantity_NOC_BLACK, Aspect_TOL_SOLID, 1.0f,
            mat, mat
        });
    }

    m_viewCube->Attributes()->SetDatumAspect(aspect);

    m_interactiveContext->Display(m_viewCube, false);

    SetLockOrbitZUp(true);
    SetAllowPanning(true);

    ChangeMouseGestureMap().Bind(Aspect_VKeyMouse_LeftButton, AIS_MouseGesture::AIS_MouseGesture_RotateOrbit);
    ChangeMouseGestureMap().Bind(Aspect_VKeyMouse_MiddleButton, AIS_MouseGesture::AIS_MouseGesture_RotateView);
    ChangeMouseGestureMap().Bind(Aspect_VKeyMouse_RightButton, AIS_MouseGesture::AIS_MouseGesture_Pan);

    //Handle(Prs3d_LineAspect) line = new Prs3d_LineAspect(Quantity_NOC_WHITE, Aspect_TOL_SOLID, 2.0);
    m_interactiveContext->HighlightStyle()->SetColor(Quantity_NOC_WHITE);
    //m_interactiveContext->HighlightStyle()->SetLineAspect(line);
}

void OcctRenderer::paint() {
    auto pos = m_parent->mapToScene({0, 0});
    int width = m_parent->width(), height = m_parent->height(), windowHeight = m_parent->window()->height();

    m_parent->window()->beginExternalCommands();

    width = std::clamp(width, 1, 4096);
    height = std::clamp(height, 1, 4096);

    auto glContext = m_driver->GetSharedContext();

    if (m_window->SetSize(width, height)) {
        m_fbo->Init(glContext, Graphic3d_Vec2i{width, height}, GL_RGBA8, GL_DEPTH24_STENCIL8, 0);
        //m_fbo->ChangeViewport(width, height);
        m_view->MustBeResized();
        m_view->Invalidate();
    }

    glContext->Functions()->glDisable(GL_BLEND);

    m_fbo->BindBuffer(glContext);
    m_view->InvalidateImmediate();
    FlushViewEvents(m_interactiveContext, m_view, true);
    m_fbo->UnbindBuffer(glContext);

    m_fbo->BindReadBuffer(glContext);

    glContext->Functions()->glBlitFramebuffer(
        0, 0, width, height,
        pos.x(), windowHeight - pos.y() - height, pos.x() + width, windowHeight - pos.y(),
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST);

    m_fbo->UnbindBuffer(glContext);

    m_parent->window()->endExternalCommands();

    if (m_viewCube->HasAnimation()) {
        m_parent->window()->update();
    }
}

void OcctRenderer::setResult(BackgroundExecutorResult *result) {
    if (!result->shapes()) {
        return;
    }

    bool center = m_shapes.empty();

    for (const auto &sh : m_shapes) {
        m_interactiveContext->Remove(sh, false);
    }

    m_shapes.clear();

    for (const auto &sh : *result->shapes()) {
        Handle(ShapeOwner) owner = new ShapeOwner();
        std::transform(sh.spans().cbegin(), sh.spans().cend(), std::back_inserter(owner->spans), [](const auto &s) { return SpanObj(s); });

        Handle(AIS_Shape) aisShape = new AIS_Shape(sh.shape());
        aisShape->Attributes()->SetFaceBoundaryDraw(true);

        if (sh.hasProp("highlight")) {
            owner->isHighlight = true;

            aisShape->SetColor(Quantity_Color{Quantity_NOC_RED});
            aisShape->SetTransparency();
        } else if (auto pcolor = sh.getProp("color").as<std::string>()) {
            auto colorName = pcolor->c_str();

            Quantity_Color color;
            if (Quantity_Color::ColorFromHex(colorName, color)) {
                aisShape->SetColor(color);
            } else if (Quantity_Color::ColorFromName(colorName, color)) {
                aisShape->SetColor(color);
            }
        }

        Handle(Prs3d_LineAspect) line = new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0);
        aisShape->Attributes()->SetFaceBoundaryAspect(line);
        //aisShape->Attributes()->SetFaceBoundaryDraw(false);
        aisShape->Attributes()->SetLineAspect(line);

        aisShape->SetOwner(owner);

        m_shapes.push_back(aisShape);
    }

    updateView();

    if (center) {
        m_view->SetProj(V3d_XnegYnegZpos, false);
        m_view->FitMinMax(m_view->Camera(), m_view->View()->MinMaxValues(), 0.01);
    }
}

void OcctRenderer::setHoveredPosition(int position) {
    QList<SpanObj> hoveredSpans;
    for (const auto &aisShape : m_shapes) {
        auto owner = Handle(ShapeOwner)::DownCast(aisShape->GetOwner());
        if (owner->isHovered) {
            hoveredSpans.append(owner->spans);
        }

        bool match = false;
        for (const auto &span : owner->spans) {
            if (span.begin <= position && position < span.end) {
                match = true;
                break;
            }
        }

        if (match != owner->isHovered) {
            owner->isHovered = match;

            if (m_showHighlightedShapes || !owner->isHighlight) {
                Handle(Prs3d_LineAspect) line = new Prs3d_LineAspect(owner->isHovered ? Quantity_NOC_WHITE : Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0);
                aisShape->Attributes()->SetFaceBoundaryAspect(line);
                aisShape->Attributes()->SetLineAspect(line);
                aisShape->Attributes()->SetColor(owner->isHovered ? Quantity_NOC_WHITE : Quantity_NOC_BLACK);

                m_interactiveContext->Remove(aisShape, false);
                m_interactiveContext->Display(aisShape, AIS_Shaded, TopAbs_SHAPE, false);
            }
        }
    }

    m_view->Invalidate();

    updateHoveredSpans();
}

void OcctRenderer::setShowHighlightedShapes(bool show) {
    m_showHighlightedShapes = show;

    updateView();
}

void OcctRenderer::updateView() {
    for (const auto &aisShape : m_shapes) {
        auto owner = Handle(ShapeOwner)::DownCast(aisShape->GetOwner());

        if (m_showHighlightedShapes || !owner->isHighlight) {
            m_interactiveContext->Display(aisShape, AIS_Shaded, TopAbs_SHAPE, false);
        } else {
            m_interactiveContext->Remove(aisShape, false);
        }
    }

    m_view->Invalidate();

    updateHoveredSpans();
}

void OcctRenderer::updateHoveredSpans() {
    QList<SpanObj> hoveredSpans;
    for (const auto &aisShape : m_shapes) {
        auto owner = Handle(ShapeOwner)::DownCast(aisShape->GetOwner());
        if (owner->isHovered) {
            hoveredSpans.append(owner->spans);
        }
    }
    emit hoveredSpansChanged(hoveredSpans);
}

#include "occtview.moc"
