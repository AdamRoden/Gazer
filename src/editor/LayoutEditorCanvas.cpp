#include "editor/LayoutEditorCanvas.h"

#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

namespace gazer {

LayoutEditorCanvas::LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(QStringLiteral("Scroll to zoom · Middle-drag or Space-drag to pan · Alt-drag empty "
                              "board to move the grid"));
    m_testTimer = new QTimer(this);
    m_testTimer->setInterval(16);
    connect(m_testTimer, &QTimer::timeout, this, &LayoutEditorCanvas::tickTestDwell);
    connect(&m_session, &LayoutEditorSession::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::placeKindChanged, this, [this]() { update(); });
    auto bindScreens = [this]() {
        for (QScreen* s : QGuiApplication::screens()) {
            if (!s) {
                continue;
            }
            connect(s, &QScreen::geometryChanged, this, QOverload<>::of(&QWidget::update),
                    Qt::UniqueConnection);
            connect(s, &QScreen::availableGeometryChanged, this, QOverload<>::of(&QWidget::update),
                    Qt::UniqueConnection);
        }
    };
    bindScreens();
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [bindScreens]() { bindScreens(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, QOverload<>::of(&QWidget::update));
}

void LayoutEditorCanvas::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    update();
}

void LayoutEditorCanvas::setShowGrid(bool on)
{
    m_showGrid = on;
    update();
}

void LayoutEditorCanvas::setTestMode(bool on)
{
    m_testMode = on;
    m_testProgress = 0.0;
    if (on) {
        m_testTimer->start();
    } else {
        m_testTimer->stop();
    }
    update();
}

void LayoutEditorCanvas::tickTestDwell()
{
    if (!m_testMode || m_hoverId.isEmpty()) {
        return;
    }
    int holdMs = 800;
    if (const PageLeaf* leaf = m_session.itemById(m_hoverId)) {
        if (leaf->dwell.activation && !leaf->dwell.activation->isEmpty()) {
            holdMs = qMax(1, leaf->dwell.activation->first());
        }
    } else if (m_session.document().dwell.activation
               && !m_session.document().dwell.activation->isEmpty()) {
        holdMs = qMax(1, m_session.document().dwell.activation->first());
    }
    m_testProgress += 16.0 / double(holdMs);
    if (m_testProgress >= 1.0) {
        m_testProgress = 0.0;
    }
    update();
}

QRectF LayoutEditorCanvas::targetRect(const PageTarget& t)
{
    QRectF r = t.geom.contentOnScreen();
    if (r.isEmpty()) {
        r = t.geom.visual;
    }
    return r;
}

QRectF LayoutEditorCanvas::gridVisual(const ScreenMap& m, const QString& gridId) const
{
    for (const PageGridPaint& gp : m.grids) {
        if (gp.gridId == gridId) {
            return gp.visual;
        }
    }
    return {};
}

QRectF LayoutEditorCanvas::virtContentRect(const ScreenMap& m) const
{
    const PageGrid* g = m_session.selectedGrid();
    if (g) {
        const QRectF visual = gridVisual(m, g->id);
        if (!visual.isEmpty()) {
            return visual;
        }
        const QRectF b = PageHit::gridBounds(*g, m.pageFrame());
        if (!b.isEmpty()) {
            return b;
        }
    }
    QRectF u;
    for (const PageGridPaint& gp : m.grids) {
        if (!gp.visual.isEmpty()) {
            u = u.isEmpty() ? gp.visual : u.united(gp.visual);
        }
    }
    for (const PageTarget& t : m.targets) {
        const QRectF r = targetRect(t);
        if (!r.isEmpty()) {
            u = u.isEmpty() ? r : u.united(r);
        }
    }
    if (u.isEmpty()) {
        return QRectF(QPointF(0, 0), QSizeF(m.virtualScreen));
    }
    return u;
}

void LayoutEditorCanvas::fitToVirt(const QRectF& virt)
{
    QRectF r = virt;
    if (r.width() < 8.0 || r.height() < 8.0) {
        r = QRectF(0, 0, 1920, 1080);
    }
    const QRectF view = QRectF(rect()).adjusted(32, 32, -32, -32);
    if (view.width() < 16.0 || view.height() < 16.0) {
        return;
    }
    const double s = qMin(view.width() / r.width(), view.height() / r.height());
    m_scale = qBound(0.05, s, 8.0);
    m_lookAt = r.center();
    emit zoomChanged(m_scale);
    update();
}

void LayoutEditorCanvas::ensureCamera()
{
    if (m_scale > 0.0 || width() < 40 || height() < 40) {
        return;
    }
    fitGrid();
}

void LayoutEditorCanvas::fitGrid()
{
    fitToVirt(virtContentRect(map()));
}

void LayoutEditorCanvas::fitScreen()
{
    const ScreenMap m = map();
    fitToVirt(QRectF(QPointF(0, 0), QSizeF(m.virtualScreen)));
}

void LayoutEditorCanvas::zoomBy(double factor)
{
    zoomAt(rect().center(), factor);
}

void LayoutEditorCanvas::zoomAt(const QPoint& canvasPos, double factor)
{
    if (m_scale <= 0.0) {
        ensureCamera();
    }
    if (m_scale <= 0.0) {
        return;
    }
    const ScreenMap m = map();
    const QPointF virt = toVirt(canvasPos, m);
    m_scale = qBound(0.05, m_scale * factor, 8.0);
    const QPointF center = QRectF(rect()).center();
    m_lookAt = virt - QPointF((canvasPos.x() - center.x()) / m_scale,
                              (canvasPos.y() - center.y()) / m_scale);
    emit zoomChanged(m_scale);
    update();
}

LayoutEditorCanvas::ScreenMap LayoutEditorCanvas::map() const
{
    ScreenMap m;
    QRect screenGeo = overlayScreenGeometry();
    if (screenGeo.isEmpty()) {
        screenGeo = QRect(0, 0, 1920, 1080);
    }
    m.virtualScreen = screenGeo.size();
    m.virtualDesktop = overlayDesktopLocal();
    if (m.virtualDesktop.size().isEmpty()) {
        m.virtualDesktop = QRect(QPoint(0, 0), m.virtualScreen);
    }
    m.taskbars = reservedStrips(QRect(QPoint(0, 0), m.virtualScreen), m.virtualDesktop);

    const PageFrame frame = m.pageFrame();
    m.targets = PageHit::collect(m_session.document(), frame, {}, false, &m.grids, true);

    double s = m_scale;
    QPointF lookAt = m_lookAt;
    if (s <= 0.0) {
        const QRectF view = QRectF(rect()).adjusted(32, 32, -32, -32);
        const QSizeF vs(m.virtualScreen);
        if (view.width() > 8.0 && vs.width() > 0.0) {
            s = qMin(view.width() / vs.width(), view.height() / vs.height());
        } else {
            s = 0.25;
        }
        lookAt = QPointF(vs.width() / 2.0, vs.height() / 2.0);
    }
    m.scaleX = s;
    m.scaleY = s;
    const QSizeF screenSize(m.virtualScreen.width() * s, m.virtualScreen.height() * s);
    const QPointF center = QRectF(rect()).center();
    m.screen = QRectF(center.x() - lookAt.x() * s, center.y() - lookAt.y() * s, screenSize.width(),
                      screenSize.height());
    m.glass = m.screen.adjusted(-8, -8, 8, 8);
    m.bezel = m.glass.adjusted(-10, -10, 10, 10);

    QRectF boardVirt;
    const PageGrid* fit = m_session.selectedGrid();
    if (fit) {
        boardVirt = gridVisual(m, fit->id);
        if (boardVirt.isEmpty()) {
            boardVirt = PageHit::gridBounds(*fit, frame);
        }
    }
    m.boardVirt = boardVirt;
    m.board = boardVirt.isEmpty()
                  ? m.screen
                  : QRectF(m.fromVirt(boardVirt.topLeft()),
                           QSizeF(boardVirt.width() * s, boardVirt.height() * s));
    m.virtBoard = boardVirt.size().toSize();
    return m;
}

QRectF LayoutEditorCanvas::toCanvas(const QRectF& virt, const ScreenMap& m) const
{
    return QRectF(m.fromVirt(virt.topLeft()),
                  QSizeF(virt.width() * m.scaleX, virt.height() * m.scaleY));
}

QPointF LayoutEditorCanvas::toVirt(const QPoint& pos, const ScreenMap& m) const
{
    return QPointF((pos.x() - m.screen.left()) / qMax(0.001, m.scaleX),
                   (pos.y() - m.screen.top()) / qMax(0.001, m.scaleY));
}

QPoint LayoutEditorCanvas::virtFromCanvas(const QPoint& pos, const ScreenMap& m) const
{
    return QPoint(int((pos.x() - m.screen.left()) / qMax(0.001, m.scaleX)),
                  int((pos.y() - m.screen.top()) / qMax(0.001, m.scaleY)));
}

QHash<QString, QRectF> LayoutEditorCanvas::boardItemRects(const ScreenMap& m) const
{
    QHash<QString, QRectF> out;
    for (const PageTarget& target : m.targets) {
        const QRectF r = targetRect(target);
        if (!r.isEmpty()) {
            out.insert(target.id, r);
        }
    }
    return out;
}

void LayoutEditorCanvas::paintEvent(QPaintEvent*)
{
    ensureCamera();
    QPainter p(this);
    p.fillRect(rect(), m_theme.bgMain);
    const ScreenMap m = map();
    paintMonitor(p, m);
    paintBoard(p, m);
    paintPlacementPip(p, m);
    paintGuides(p, m);
    if (m_drag == Drag::Rubber) {
        p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
        p.setBrush(QColor(m_theme.accent.red(), m_theme.accent.green(), m_theme.accent.blue(), 40));
        p.drawRect(m_rubber.normalized());
    }
}

void LayoutEditorCanvas::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }
    zoomAt(event->position().toPoint(), delta > 0 ? 1.12 : 1.0 / 1.12);
    event->accept();
}

void LayoutEditorCanvas::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensureCamera();
}

void LayoutEditorCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    ensureCamera();
}

} // namespace gazer
