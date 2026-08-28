#include "editor/LayoutEditorCanvas.h"

#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "ui/BoardPaint.h"
#include "utils/ScreenGrab.h"

#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QTimer>

namespace gazer {

LayoutEditorCanvas::LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
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

void LayoutEditorCanvas::setFitBoard(bool on)
{
    m_fitBoard = on;
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
    const QRectF box = QRectF(rect()).adjusted(24, 24, -24, -24);
    const double sx = box.width() / m.virtualScreen.width();
    const double sy = box.height() / m.virtualScreen.height();
    const double s = m_fitBoard ? qMin(sx, sy) * 1.15 : qMin(sx, sy);
    m.scaleX = s;
    m.scaleY = s;
    const QSizeF screenSize(m.virtualScreen.width() * s, m.virtualScreen.height() * s);
    m.screen = QRectF(QPointF(box.center().x() - screenSize.width() / 2.0,
                              box.center().y() - screenSize.height() / 2.0),
                      screenSize);
    m.glass = m.screen.adjusted(-8, -8, 8, 8);
    m.bezel = m.glass.adjusted(-10, -10, 10, 10);

    const PageFrame frame = m.pageFrame();
    m.targets = PageHit::collect(m_session.document(), frame, {}, {}, false, &m.grids, true);
    QRectF boardVirt;
    const PageGrid* fit = m_session.selectedGrid();
    if (fit) {
        for (const PageGridPaint& g : m.grids) {
            if (g.gridId == fit->id) {
                boardVirt = g.visual;
                break;
            }
        }
        if (boardVirt.isEmpty()) {
            boardVirt = PageHit::gridBounds(*fit, frame);
        }
    }
    m.board = boardVirt.isEmpty() ? m.screen
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

QHash<QString, QRectF> LayoutEditorCanvas::boardItemRects(const ScreenMap& m) const
{
    QHash<QString, QRectF> out;
    for (const PageTarget& target : m.targets) {
        QRectF r = target.geom.contentOnScreen();
        if (r.isEmpty()) {
            r = target.geom.visual;
        }
        if (!r.isEmpty()) {
            out.insert(target.id, r);
        }
    }
    return out;
}

QRectF LayoutEditorCanvas::unboundedCanvasRect(const QString& id, const ScreenMap& m) const
{
    for (const PageTarget& t : m.targets) {
        if (t.id == id) {
            QRectF r = t.geom.contentOnScreen();
            if (r.isEmpty()) {
                r = t.geom.visual;
            }
            return toCanvas(r, m);
        }
    }
    return {};
}

QString LayoutEditorCanvas::hitItem(const QPoint& pos, const ScreenMap& m) const
{
    for (int i = m.targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = m.targets[i];
        QRectF r = t.geom.contentOnScreen();
        if (r.isEmpty()) {
            r = t.geom.visual;
        }
        if (!r.isEmpty() && toCanvas(r, m).contains(pos)) {
            return t.id;
        }
        if (t.kind == PageTarget::Kind::Zone && !t.geom.dwellZone.isEmpty()
            && toCanvas(t.geom.dwellZone, m).contains(pos)) {
            return t.id;
        }
    }
    return {};
}

bool LayoutEditorCanvas::hitBoard(const QPoint& pos, const ScreenMap& m) const
{
    const QPointF virt = toVirt(pos, m);
    for (int i = m.grids.size() - 1; i >= 0; --i) {
        if (m.grids[i].visual.contains(virt)) {
            return true;
        }
    }
    return false;
}

QPoint LayoutEditorCanvas::cellAt(const QPoint& pos, const ScreenMap& m, QString* gridId) const
{
    const QPointF virt = toVirt(pos, m);
    for (int i = m.grids.size() - 1; i >= 0; --i) {
        const PageGridPaint& gp = m.grids[i];
        if (!gp.visual.contains(virt)) {
            continue;
        }
        const PageGrid* g = PageEdit::findGrid(m_session.document(), gp.gridId);
        if (!g) {
            continue;
        }
        if (gridId) {
            *gridId = gp.gridId;
        }
        const QPoint idx = PageHit::cellIndexAt(*g, gp.visual, virt);
        if (idx.x() < 0) {
            return {0, 0};
        }
        return idx;
    }
    if (gridId) {
        gridId->clear();
    }
    return {0, 0};
}

void LayoutEditorCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), m_theme.bgMain);
    const ScreenMap m = map();
    paintMonitor(p, m);
    paintBoard(p, m);
    paintPlacementPip(p, m);
    if (m_drag == Drag::Rubber) {
        p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
        p.setBrush(QColor(m_theme.accent.red(), m_theme.accent.green(), m_theme.accent.blue(), 40));
        p.drawRect(m_rubber.normalized());
    }
}

void LayoutEditorCanvas::paintPlacementPip(QPainter& p, const ScreenMap& m) const
{
    if (!m_session.placeKind()) {
        return;
    }
    p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(m.board);
}

void LayoutEditorCanvas::paintMonitor(QPainter& p, const ScreenMap& m) const
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 22, 24));
    p.drawRoundedRect(m.bezel, 10, 10);
    p.setBrush(QColor(8, 8, 10));
    p.drawRoundedRect(m.glass, 6, 6);
    p.setBrush(QColor(12, 14, 16));
    p.drawRect(m.screen);
    paintTaskbar(p, m);
}

void LayoutEditorCanvas::paintTaskbar(QPainter& p, const ScreenMap& m) const
{
    if (m.taskbars.isEmpty()) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(m.screen);
    const QRectF desk = m.fromVirt(m.virtualDesktop);
    for (const QRect& virt : m.taskbars) {
        const QRectF bar = m.fromVirt(virt);
        if (bar.width() < 1.0 || bar.height() < 1.0) {
            continue;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(32, 32, 38));
        p.drawRect(bar);

        const bool horizontal = bar.width() >= bar.height();
        p.setPen(QPen(QColor(255, 255, 255, 32), 1));
        if (horizontal) {
            if (bar.center().y() >= desk.center().y()) {
                p.drawLine(bar.topLeft() + QPointF(0, 0.5), bar.topRight() + QPointF(0, 0.5));
            } else {
                p.drawLine(bar.bottomLeft() + QPointF(0, -0.5),
                           bar.bottomRight() + QPointF(0, -0.5));
            }
        } else if (bar.center().x() >= desk.center().x()) {
            p.drawLine(bar.topLeft() + QPointF(0.5, 0), bar.bottomLeft() + QPointF(0.5, 0));
        } else {
            p.drawLine(bar.topRight() + QPointF(-0.5, 0), bar.bottomRight() + QPointF(-0.5, 0));
        }

        const double thick = horizontal ? bar.height() : bar.width();
        const double icon = qBound(5.0, thick * 0.42, 16.0);
        const double gap = icon * 0.38;
        constexpr int kIcons = 5;
        const double span = kIcons * icon + (kIcons - 1) * gap;
        const QPointF origin = horizontal
            ? QPointF(bar.center().x() - span / 2.0, bar.center().y() - icon / 2.0)
            : QPointF(bar.center().x() - icon / 2.0, bar.center().y() - span / 2.0);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < kIcons; ++i) {
            const QRectF r = horizontal
                ? QRectF(origin.x() + i * (icon + gap), origin.y(), icon, icon)
                : QRectF(origin.x(), origin.y() + i * (icon + gap), icon, icon);
            p.setBrush(i == 0 ? QColor(76, 194, 255, 210) : QColor(255, 255, 255, 38 + i * 10));
            p.drawRoundedRect(r, qMin(3.0, icon * 0.28), qMin(3.0, icon * 0.28));
        }
    }
    p.restore();
}

void LayoutEditorCanvas::paintBoard(QPainter& p, const ScreenMap& m) const
{
    const PageFrame frame = m.pageFrame();
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> targets =
        PageHit::collect(m_session.document(), frame, {}, {}, false, &grids, true);
    auto toCanvas = [&](const QRectF& virt) {
        return QRectF(m.fromVirt(virt.topLeft()),
                      QSizeF(virt.width() * m.scaleX, virt.height() * m.scaleY));
    };
    if (m_showGrid && !m.board.isEmpty()) {
        p.setPen(QPen(QColor(255, 255, 255, 28), 1));
        const PageGrid* g = PageEdit::primaryGrid(m_session.document());
        const int cols = g ? qMax(1, g->columns) : 4;
        const int rows = g ? qMax(1, g->rows) : 2;
        for (int c = 1; c < cols; ++c) {
            const double x = m.board.left() + m.board.width() * c / cols;
            p.drawLine(QPointF(x, m.board.top()), QPointF(x, m.board.bottom()));
        }
        for (int r = 1; r < rows; ++r) {
            const double y = m.board.top() + m.board.height() * r / rows;
            p.drawLine(QPointF(m.board.left(), y), QPointF(m.board.right(), y));
        }
    }
    for (const PageGridPaint& g : grids) {
        BoardPaint::paintSurface(p, toCanvas(g.visual), g.chrome, m_theme, nullptr, true, false,
                                 false, false);
    }
    for (const PageTarget& t : targets) {
        const bool hovered = t.id == m_hoverId;
        const bool selected = m_session.isItemSelected(t.id);
        if (t.kind == PageTarget::Kind::Zone) {
            const QRectF dwell = toCanvas(t.geom.dwellZone);
            const QRectF progress =
                toCanvas(t.geom.visual.isEmpty() ? t.geom.progressZone : t.geom.visual);
            if (!dwell.isEmpty() && dwell != progress) {
                QColor fill = m_theme.accent;
                fill.setAlpha(28);
                p.setBrush(fill);
                p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
                p.drawRect(dwell);
                const QPolygonF hull = PageHit::gazeHitPolygon(t, sessionKey(t));
                if (hull.size() >= 3) {
                    QPolygonF canvasHull;
                    canvasHull.reserve(hull.size());
                    for (const QPointF& pt : hull) {
                        canvasHull << m.fromVirt(pt);
                    }
                    p.setBrush(Qt::NoBrush);
                    QColor gap = m_theme.accent;
                    gap.setAlpha(90);
                    p.setPen(QPen(gap, 1, Qt::DotLine));
                    p.drawPolygon(canvasHull);
                }
            }
            if (!progress.isEmpty()) {
                BoardPaint::paintTarget(p, t, progress, m_theme, nullptr, hovered,
                                        hovered ? m_testProgress : 0.0, false, selected, {}, {}, {},
                                        0, {}, 0);
            }
            if (selected) {
                if (!progress.isEmpty()) {
                    paintSelectionOverlay(p, progress, true);
                }
                if (!dwell.isEmpty() && dwell != progress) {
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(m_theme.accent, 2, Qt::DashLine));
                    p.drawRect(dwell.adjusted(1, 1, -1, -1));
                }
            }
            continue;
        }
        QRectF r = toCanvas(t.geom.contentOnScreen());
        if (r.isEmpty()) {
            r = toCanvas(t.geom.visual);
        }
        BoardPaint::paintTarget(p, t, r, m_theme, nullptr, hovered, hovered ? m_testProgress : 0.0,
                                false, selected, {}, {}, {}, 0, {}, 0);
        if (selected) {
            paintSelectionOverlay(p, r, true);
        }
    }
}

void LayoutEditorCanvas::paintSelectionOverlay(QPainter& p, const QRectF& r, bool handles) const
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_theme.accent, 2));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 6, 6);
    if (handles) {
        paintHandles(p, r);
    }
}

void LayoutEditorCanvas::paintHandles(QPainter& p, const QRectF& r) const
{
    p.setBrush(m_theme.accent);
    p.setPen(Qt::NoPen);
    p.drawRect(QRectF(r.right() - 5, r.center().y() - 5, 10, 10));
    p.drawRect(QRectF(r.center().x() - 5, r.bottom() - 5, 10, 10));
}

QRectF LayoutEditorCanvas::selectedRect(const ScreenMap& m) const
{
    const QString id = m_session.selection().itemId;
    if (id.isEmpty()) {
        return {};
    }
    const auto rects = boardItemRects(m);
    const QRectF virt = rects.value(id);
    if (virt.isEmpty()) {
        return {};
    }
    return QRectF(m.fromVirt(virt.topLeft()),
                  QSizeF(virt.width() * m.scaleX, virt.height() * m.scaleY));
}

QPoint LayoutEditorCanvas::virtFromCanvas(const QPoint& pos, const ScreenMap& m) const
{
    return QPoint(int((pos.x() - m.screen.left()) / qMax(0.001, m.scaleX)),
                  int((pos.y() - m.screen.top()) / qMax(0.001, m.scaleY)));
}

LayoutEditorCanvas::Drag LayoutEditorCanvas::hitHandle(const QPoint& pos, const ScreenMap& m) const
{
    const QRectF r = selectedRect(m);
    if (r.isEmpty()) {
        return Drag::None;
    }
    const QRectF hr(r.right() - 7, r.center().y() - 7, 14, 14);
    const QRectF hb(r.center().x() - 7, r.bottom() - 7, 14, 14);
    if (hr.contains(pos)) {
        return Drag::ResizeW;
    }
    if (hb.contains(pos)) {
        return Drag::ResizeH;
    }
    return Drag::None;
}

void LayoutEditorCanvas::updateDragCursor(const ScreenMap& m, const QPoint& pos)
{
    if (m_drag == Drag::ResizeW || hitHandle(pos, m) == Drag::ResizeW) {
        setCursor(Qt::SizeHorCursor);
    } else if (m_drag == Drag::ResizeH || hitHandle(pos, m) == Drag::ResizeH) {
        setCursor(Qt::SizeVerCursor);
    } else if (m_drag == Drag::Grid) {
        setCursor(Qt::SizeAllCursor);
    } else if (m_session.placeKind()) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void LayoutEditorCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    const ScreenMap m = map();
    m_pressPos = event->pos();
    m_dragPos = event->pos();
    m_drag = Drag::None;
    m_dragId.clear();

    const Drag handle = hitHandle(event->pos(), m);
    if (handle != Drag::None) {
        m_drag = handle;
        m_dragId = m_session.selection().itemId;
        m_pressRect = boardItemRects(m).value(m_dragId);
        return;
    }

    const QString id = hitItem(event->pos(), m);
    if (!id.isEmpty()) {
        m_drag = Drag::Move;
        m_dragId = id;
        m_session.selectItem(id, event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
        return;
    }

    if (m_session.placeKind() && *m_session.placeKind() == EditorItemKind::Zone) {
        m_session.addZoneAt(virtFromCanvas(event->pos(), m));
        return;
    }
    if (!hitBoard(event->pos(), m)) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    if (m_session.placeKind()) {
        QString gridId;
        const QPoint cell = cellAt(event->pos(), m, &gridId);
        m_session.addItemAt(*m_session.placeKind(), cell.y(), cell.x(), gridId);
        return;
    }
    QString gridId;
    (void)cellAt(event->pos(), m, &gridId);
    if (!gridId.isEmpty()) {
        m_session.selectGrid(gridId);
    }
    if (!m_fitBoard) {
        m_drag = Drag::Grid;
        return;
    }
    m_drag = Drag::Rubber;
    m_rubber = QRect(event->pos(), QSize(1, 1));
}

void LayoutEditorCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const ScreenMap m = map();
    m_dragPos = event->pos();
    const QString hover = hitItem(event->pos(), m);
    if (hover != m_hoverId) {
        m_hoverId = hover;
        m_testProgress = 0.0;
    }
    if (m_drag == Drag::Rubber) {
        m_rubber = QRect(m_pressPos, event->pos()).normalized();
    }
    updateDragCursor(m, event->pos());
    update();
}

void LayoutEditorCanvas::commitDrag(const QPoint& pos, const ScreenMap& m)
{
    switch (m_drag) {
    case Drag::Move:
        if (!m_dragId.isEmpty() && (pos - m_pressPos).manhattanLength() > 8) {
            if (PageEdit::isZone(m_session.document(), m_dragId)) {
                const double dx = (pos.x() - m_pressPos.x()) / qMax(0.001, m.scaleX);
                const double dy = (pos.y() - m_pressPos.y()) / qMax(0.001, m.scaleY);
                const QString id = m_dragId;
                m_session.edit(QStringLiteral("Move zone"), [&](PageDocument& d) {
                    if (PageZone* z = PageEdit::findZone(d, id)) {
                        const double x0 = z->offset.x.isSet() ? z->offset.x.value : 0.0;
                        const double y0 = z->offset.y.isSet() ? z->offset.y.value : 0.0;
                        z->offset.x = PageDim::pixels(x0 + dx);
                        z->offset.y = PageDim::pixels(y0 + dy);
                    }
                });
            } else if (m.board.contains(pos)) {
                const QPoint cell = cellAt(pos, m);
                const PageCell* c = PageEdit::findCell(m_session.document(), m_dragId);
                const int dRow = c ? cell.y() - c->row : 0;
                const int dCol = c ? cell.x() - c->col : 0;
                if (m_session.selection().itemIds.size() > 1) {
                    m_session.moveSelected(dRow, dCol);
                } else {
                    m_session.moveItemToCell(m_dragId, cell.y(), cell.x());
                }
            }
        }
        break;
    case Drag::ResizeW:
    case Drag::ResizeH:
        applyResize(pos, m);
        break;
    case Drag::Rubber: {
        const auto rects = boardItemRects(m);
        const QRectF band = QRectF(m_rubber.normalized());
        QStringList ids;
        for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
            const QRectF canvas = QRectF(m.fromVirt(it->topLeft()),
                                         QSizeF(it->width() * m.scaleX, it->height() * m.scaleY));
            if (canvas.intersects(band)) {
                ids.push_back(it.key());
            }
        }
        m_session.selectItems(ids);
        m_rubber = {};
        break;
    }
    case Drag::Grid: {
        const QPointF delta = pos - m_pressPos;
        QPoint topLeft = virtFromCanvas(m.board.topLeft().toPoint(), m);
        topLeft += QPoint(int(delta.x() / qMax(0.001, m.scaleX)),
                          int(delta.y() / qMax(0.001, m.scaleY)));
        QSize place = m.virtualScreen;
        QPoint origin(0, 0);
        const PageGrid* g = m_session.selectedGrid();
        if (g && g->desktopMode) {
            place = m.virtualDesktop.size();
            origin = m.virtualDesktop.topLeft();
        }
        m_session.snapWindowTo(topLeft - origin, place);
        break;
    }
    case Drag::None:
        break;
    }
}

void LayoutEditorCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    commitDrag(event->pos(), map());
    m_drag = Drag::None;
    m_dragId.clear();
    m_dragPos = event->pos();
    update();
}

void LayoutEditorCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    const PageLeaf* item = id.isEmpty() ? nullptr : m_session.itemById(id);
    if (!item) {
        return;
    }
    bool ok = false;
    const QString label = QInputDialog::getText(this, QStringLiteral("Key label"),
                                                QStringLiteral("Text"), QLineEdit::Normal,
                                                item->label, &ok);
    if (ok) {
        m_session.setItemLabel(id, label);
    }
}

void LayoutEditorCanvas::applyResize(const QPoint& pos, const ScreenMap& m)
{
    if (PageEdit::isZone(m_session.document(), m_dragId)) {
        const QRectF r = unboundedCanvasRect(m_dragId, m);
        if (r.width() < 1 || r.height() < 1) {
            return;
        }
        double w = qMax(16.0, r.width() / qMax(0.001, m.scaleX));
        double h = qMax(16.0, r.height() / qMax(0.001, m.scaleY));
        if (m_drag == Drag::ResizeW) {
            w = qMax(16.0, (pos.x() - r.left()) / qMax(0.001, m.scaleX));
        } else {
            h = qMax(16.0, (pos.y() - r.top()) / qMax(0.001, m.scaleY));
        }
        m_session.resizeFreeItem(m_dragId, PageDim::pixels(w), PageDim::pixels(h));
        return;
    }
    const PageCell* cell = PageEdit::findCell(m_session.document(), m_dragId);
    if (!cell) {
        return;
    }
    const QPoint c = cellAt(pos, m);
    int colSpan = cell->colSpan;
    int rowSpan = cell->rowSpan;
    if (m_drag == Drag::ResizeW) {
        colSpan = qMax(1, c.x() - cell->col + 1);
    } else {
        rowSpan = qMax(1, c.y() - cell->row + 1);
    }
    m_session.resizeItem(m_dragId, rowSpan, colSpan, 0);
}

void LayoutEditorCanvas::keyPressEvent(QKeyEvent* event)
{
    const int step = event->modifiers() & Qt::ShiftModifier ? 16 : 1;
    if (event->key() == Qt::Key_Left) {
        m_session.nudgeSelected(0, -1, step);
    } else if (event->key() == Qt::Key_Right) {
        m_session.nudgeSelected(0, 1, step);
    } else if (event->key() == Qt::Key_Up) {
        m_session.nudgeSelected(-1, 0, step);
    } else if (event->key() == Qt::Key_Down) {
        m_session.nudgeSelected(1, 0, step);
    } else if (event->key() == Qt::Key_Delete) {
        m_session.deleteSelected();
    } else if (event->key() == Qt::Key_Escape) {
        m_session.setPlaceKind(std::nullopt);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void LayoutEditorCanvas::leaveEvent(QEvent*)
{
    m_hoverId.clear();
    update();
}

void LayoutEditorCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    if (!id.isEmpty() && !m_session.isItemSelected(id)) {
        m_session.selectItem(id);
    }
    showItemMenu(event->globalPos());
}

void LayoutEditorCanvas::showItemMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    auto* dup = menu.addAction(QStringLiteral("Duplicate"));
    auto* del = menu.addAction(QStringLiteral("Delete"));
    menu.addSeparator();
    auto* toZone = menu.addAction(QStringLiteral("Convert to zone"));
    auto* toCell = menu.addAction(QStringLiteral("Convert to cell"));
    const bool has = m_session.selection().target == EditorTarget::Item;
    for (QAction* a : {dup, del, toZone, toCell}) {
        a->setEnabled(has);
    }
    QAction* chosen = menu.exec(globalPos);
    if (chosen == dup) {
        m_session.duplicateSelected();
    } else if (chosen == del) {
        m_session.deleteSelected();
    } else if (chosen == toZone) {
        m_session.convertSelectedToFree();
    } else if (chosen == toCell) {
        m_session.convertSelectedToCell();
    }
}

} // namespace gazer
