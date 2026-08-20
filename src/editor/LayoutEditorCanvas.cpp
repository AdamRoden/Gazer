#include "editor/LayoutEditorCanvas.h"

#include "layout/LayoutGeometry.h"
#include "ui/MouseIcons.h"

#include <QHash>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace gazer {

LayoutEditorCanvas::LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMinimumSize(480, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    m_testTimer = new QTimer(this);
    m_testTimer->setInterval(30);
    connect(m_testTimer, &QTimer::timeout, this, [this]() { tickTestDwell(); });
    connect(&m_session, &LayoutEditorSession::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::placeKindChanged, this, [this]() {
        setCursor(m_session.placeKind() ? Qt::CrossCursor : Qt::ArrowCursor);
    });
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
    if (!m_testMode) {
        return;
    }
    if (m_hoverId.isEmpty()) {
        m_testProgress = 0.0;
        update();
        return;
    }
    m_testProgress = qMin(1.0, m_testProgress + 0.03);
    if (m_testProgress >= 1.0) {
        if (const LayoutItem* it = m_session.itemById(m_hoverId)) {
            const auto acts = it->effectiveActions();
            QString msg = it->label;
            if (!acts.isEmpty()) {
                const LayoutAction& a = acts.first();
                if (a.type == LayoutAction::Type::TypeText) {
                    msg = QStringLiteral("Would type: %1").arg(a.text);
                } else if (a.type == LayoutAction::Type::Speak) {
                    msg = QStringLiteral("Would speak: %1").arg(a.text);
                } else if (a.type == LayoutAction::Type::Command) {
                    msg = QStringLiteral("Would run: %1").arg(a.name);
                } else if (a.type == LayoutAction::Type::OpenLayout
                           || a.type == LayoutAction::Type::LoadLayout) {
                    msg = QStringLiteral("Would open: %1").arg(a.layoutId);
                }
            }
            m_session.notify(msg);
        }
        m_testProgress = 0.0;
    }
    update();
}

LayoutEditorCanvas::ScreenMap LayoutEditorCanvas::map() const
{
    ScreenMap out;
    const QRectF area = QRectF(rect()).adjusted(28, 18, -28, -36);
    if (area.width() < 80 || area.height() < 80) {
        return out;
    }

    const qreal bezel = 14.0;
    const qreal chin = 22.0;
    QRectF glass = area;
    const qreal targetAspect = 16.0 / 9.0;
    const qreal areaAspect = area.width() / area.height();
    if (areaAspect > targetAspect) {
        const qreal w = area.height() * targetAspect;
        glass.setLeft(area.center().x() - w / 2.0);
        glass.setWidth(w);
    } else {
        const qreal h = area.width() / targetAspect;
        glass.setTop(area.center().y() - h / 2.0);
        glass.setHeight(h);
    }
    out.bezel = glass.adjusted(-bezel, -bezel, bezel, chin);
    out.glass = glass;
    out.screen = glass.adjusted(3, 3, -3, -3);

    const LayoutDocument& doc = m_session.document();
    out.virtualScreen = QSize(1920, 1080);
    const QRect virt(0, 0, out.virtualScreen.width(), out.virtualScreen.height());
    QRect boardVirt = LayoutGeometry::boardRectInBounds(doc, virt);
    if (!doc.showsBoardWindow()) {
        boardVirt = QRect(80, 80, 400, 200);
    }
    if (m_drag == Drag::Window) {
        const QPointF delta = m_dragPos - m_pressPos;
        const double scale = out.screen.width() / double(out.virtualScreen.width());
        boardVirt.translate(int(delta.x() / qMax(0.001, scale)),
                            int(delta.y() / qMax(0.001, scale)));
    }
    if (m_fitBoard) {
        const QRectF inner = out.screen.adjusted(18, 18, -18, -18);
        const qreal sx = inner.width() / qMax(1, boardVirt.width());
        const qreal sy = inner.height() / qMax(1, boardVirt.height());
        out.scale = qMin(sx, sy);
        const qreal bw = boardVirt.width() * out.scale;
        const qreal bh = boardVirt.height() * out.scale;
        out.board = QRectF(inner.center().x() - bw / 2.0, inner.center().y() - bh / 2.0, bw, bh);
    } else {
        out.scale = out.screen.width() / double(out.virtualScreen.width());
        out.board = QRectF(out.screen.left() + boardVirt.x() * out.scale,
                           out.screen.top() + boardVirt.y() * out.scale,
                           boardVirt.width() * out.scale,
                           boardVirt.height() * out.scale);
    }
    return out;
}

QString LayoutEditorCanvas::hitItem(const QPoint& pos, const ScreenMap& m) const
{
    if (!m.board.isValid()) {
        return {};
    }
    const LayoutDocument& doc = m_session.document();
    const QHash<QString, QRectF> rects =
        LayoutGeometry::itemRects(doc, int(m.board.width()), int(m.board.height()));
    const QPointF local(pos.x() - m.board.x(), pos.y() - m.board.y());
    QString best;
    double bestArea = 1e18;
    for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
        if (it.value().contains(local)) {
            const double area = it.value().width() * it.value().height();
            if (area < bestArea) {
                bestArea = area;
                best = it.key();
            }
        }
    }
    return best;
}

bool LayoutEditorCanvas::hitBoard(const QPoint& pos, const ScreenMap& m) const
{
    return m.board.contains(pos);
}

QPoint LayoutEditorCanvas::cellAt(const QPoint& pos, const ScreenMap& m) const
{
    const LayoutDocument& doc = m_session.document();
    const int cols = qMax(1, doc.grid.columns);
    const int rows = qMax(1, doc.grid.rows);
    const QPointF local(pos.x() - m.board.x(), pos.y() - m.board.y());
    const auto inset = doc.grid.insets(m.board.width(), m.board.height());
    const double innerW = m.board.width() - inset.left - inset.right;
    const double innerH = m.board.height() - inset.top - inset.bottom;
    int col = int((local.x() - inset.left) / qMax(1.0, innerW / cols));
    int row = int((local.y() - inset.top) / qMax(1.0, innerH / rows));
    col = qBound(0, col, cols - 1);
    row = qBound(0, row, rows - 1);
    return {col, row};
}

void LayoutEditorCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(QStringLiteral("#1a1a1c")));
    const ScreenMap m = map();
    if (!m.screen.isValid()) {
        return;
    }
    paintMonitor(p, m);
    paintBoard(p, m);
    if (m_fitBoard) {
        paintPlacementPip(p, m);
    }
    if (m_drag == Drag::Rubber && !m_rubber.isEmpty()) {
        p.setPen(QPen(QColor(QStringLiteral("#60cdff")), 1, Qt::DashLine));
        p.setBrush(QColor(96, 205, 255, 40));
        p.drawRect(m_rubber.normalized());
    }
}

void LayoutEditorCanvas::paintPlacementPip(QPainter& p, const ScreenMap& m) const
{
    const QRectF pip(m.screen.right() - 132, m.screen.bottom() - 82, 120, 68);
    p.setPen(QPen(QColor(QStringLiteral("#3a3a40")), 1));
    p.setBrush(QColor(15, 18, 28, 200));
    p.drawRoundedRect(pip, 4, 4);
    const LayoutDocument& doc = m_session.document();
    const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
    const QRect board = LayoutGeometry::boardRectInBounds(doc, virt);
    const qreal sx = pip.width() / double(m.virtualScreen.width());
    const qreal sy = pip.height() / double(m.virtualScreen.height());
    const QRectF br(pip.left() + board.x() * sx, pip.top() + board.y() * sy, board.width() * sx,
                    board.height() * sy);
    p.setBrush(QColor(96, 205, 255, 140));
    p.setPen(Qt::NoPen);
    p.drawRect(br);
    p.setPen(QColor(QStringLiteral("#8a90a0")));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 7));
    p.drawText(pip.adjusted(4, 2, -4, -2), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("Placement"));
}

void LayoutEditorCanvas::paintMonitor(QPainter& p, const ScreenMap& m) const
{
    QPainterPath bezel;
    bezel.addRoundedRect(m.bezel, 18, 18);
    p.fillPath(bezel, QColor(QStringLiteral("#121214")));
    p.setPen(QPen(QColor(QStringLiteral("#2a2a2e")), 1.2));
    p.drawPath(bezel);

    QLinearGradient wall(m.screen.topLeft(), m.screen.bottomRight());
    wall.setColorAt(0.0, QColor(QStringLiteral("#1b2740")));
    wall.setColorAt(0.45, QColor(QStringLiteral("#243a62")));
    wall.setColorAt(0.75, QColor(QStringLiteral("#2a4d86")));
    wall.setColorAt(1.0, QColor(QStringLiteral("#0f1728")));
    QPainterPath glass;
    glass.addRoundedRect(m.screen, 4, 4);
    p.fillPath(glass, wall);

    const QRectF standNeck(m.bezel.center().x() - 18, m.bezel.bottom() - 4, 36, 16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#2a2a2e")));
    p.drawRoundedRect(standNeck, 3, 3);
    const QRectF standBase(m.bezel.center().x() - 70, standNeck.bottom() - 2, 140, 10);
    p.drawRoundedRect(standBase, 5, 5);

    p.setPen(QColor(QStringLiteral("#8a90a0")));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
    const LayoutDocument& doc = m_session.document();
    const QString caption =
        QStringLiteral("%1  ·  %2×%3  ·  grid %4×%5")
            .arg(doc.name.isEmpty() ? doc.id : doc.name)
            .arg(int(LayoutGeometry::boardRectInBounds(doc, QRect(0, 0, 1920, 1080)).width()))
            .arg(int(LayoutGeometry::boardRectInBounds(doc, QRect(0, 0, 1920, 1080)).height()))
            .arg(doc.grid.columns)
            .arg(doc.grid.rows);
    p.drawText(QRectF(m.bezel.left(), m.bezel.bottom() + 14, m.bezel.width(), 18),
               Qt::AlignHCenter | Qt::AlignTop, caption);
}

void LayoutEditorCanvas::paintBoard(QPainter& p, const ScreenMap& m) const
{
    const LayoutDocument& doc = m_session.document();
    const QRectF board = m.board;
    if (board.width() < 4 || board.height() < 4) {
        return;
    }

    const LayoutChromeStyle winSt = doc.placement.style;
    const double winR = winSt.radius.value_or(16.0);
    QColor winBg = winSt.background.value_or(QColor(32, 36, 42, 210));
    QColor winBorder = winSt.borderColor.value_or(QColor(QStringLiteral("#3a3f46")));
    const double winBw = winSt.borderWidth.value_or(1.0);

    p.setPen(Qt::NoPen);
    p.setBrush(winBg);
    p.drawRoundedRect(board, winR, winR);
    if (winBw > 0.0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(winBorder, winBw));
        p.drawRoundedRect(board, winR, winR);
    }

    const bool windowSel = m_session.selection().target == EditorTarget::Window
                           || m_session.selection().target == EditorTarget::Grid;
    if (windowSel) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(QStringLiteral("#60cdff")), 2.0));
        p.drawRoundedRect(board.adjusted(-3, -3, 3, 3), winR + 2, winR + 2);
    }

    const int bw = qMax(1, int(board.width()));
    const int bh = qMax(1, int(board.height()));
    const QHash<QString, QRectF> rects = LayoutGeometry::itemRects(doc, bw, bh);

    if (m_showGrid) {
        p.save();
        p.translate(board.topLeft());
        p.setPen(QPen(QColor(255, 255, 255, 28), 1, Qt::DashLine));
        const auto inset = doc.grid.insets(bw, bh);
        const int cols = qMax(1, doc.grid.columns);
        const int rows = qMax(1, doc.grid.rows);
        const double innerW = bw - inset.left - inset.right;
        const double innerH = bh - inset.top - inset.bottom;
        const double cw = innerW / cols;
        const double rh = innerH / rows;
        for (int c = 0; c <= cols; ++c) {
            const double x = inset.left + c * cw;
            p.drawLine(QPointF(x, inset.top), QPointF(x, inset.top + innerH));
        }
        for (int r = 0; r <= rows; ++r) {
            const double y = inset.top + r * rh;
            p.drawLine(QPointF(inset.left, y), QPointF(inset.left + innerW, y));
        }
        p.restore();
    }

    p.save();
    p.translate(board.topLeft());
    const EditorSelection sel = m_session.selection();
    for (const LayoutItem& item : doc.items) {
        if (!item.participatesInBoardGrid()) {
            continue;
        }
        const QRectF r = rects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }
        QRectF drawR = r;
        if (m_drag == Drag::Move && item.id == m_dragId
            && (m_dragPos - m_pressPos).manhattanLength() > 8) {
            const QPoint dest = cellAt(m_dragPos, m);
            const double cw = r.width() / qMax(1, item.colSpan);
            const double rh = r.height() / qMax(1, item.rowSpan);
            drawR.translate((dest.x() - item.col) * cw, (dest.y() - item.row) * rh);
        } else if ((m_drag == Drag::ResizeW || m_drag == Drag::ResizeH) && item.id == m_dragId) {
            const QPointF local(m_dragPos.x() - m.board.x(), m_dragPos.y() - m.board.y());
            if (m_drag == Drag::ResizeW) {
                drawR.setWidth(qMax(8.0, local.x() - m_pressRect.left()));
            } else {
                drawR.setHeight(qMax(8.0, local.y() - m_pressRect.top()));
            }
        }
        const bool selected = m_session.isItemSelected(item.id);
        const bool hovered = item.id == m_hoverId;
        paintCell(p, item, drawR, selected, hovered);
        if (m_testMode && item.id == m_hoverId && m_testProgress > 0.02) {
            QPen pen(m_theme.accent, 3);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawArc(r.adjusted(3, 3, -3, -3), 90 * 16, int(-m_testProgress * 360 * 16));
        }
        if (selected && item.id == sel.itemId && !m_testMode) {
            const QRectF hr(r.right() - 5, r.center().y() - 5, 10, 10);
            const QRectF hb(r.center().x() - 5, r.bottom() - 5, 10, 10);
            p.setBrush(QColor(QStringLiteral("#60cdff")));
            p.setPen(Qt::NoPen);
            p.drawRect(hr);
            p.drawRect(hb);
        }
    }
    p.restore();

    for (const LayoutItem& item : doc.items) {
        if (item.participatesInBoardGrid()) {
            continue;
        }
            QRectF marker(board.center(), QSizeF(48, 18));
            marker.moveCenter(QPointF(m.screen.center().x(), m.screen.bottom() - 28));
            if (item.hasDwellRegion && item.dwellRegion.usesScreenAnchor()) {
                const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
                const double w = item.dwellRegion.width.resolve(virt.width());
                const double h = item.dwellRegion.height.resolve(virt.height());
                marker = QRectF(0, 0, qMax(24.0, w * m.scale), qMax(16.0, h * m.scale));
                const auto sa = item.dwellRegion.screenAnchor;
                using SA = LayoutDwellRegion::ScreenAnchor;
                QPointF c = m.screen.center();
                if (sa == SA::Top || sa == SA::TopCenter || sa == SA::TopLeft || sa == SA::TopRight) {
                    c.setY(m.screen.top() + marker.height() / 2 + 4);
                }
                if (sa == SA::Bottom || sa == SA::BottomCenter || sa == SA::BottomLeft
                    || sa == SA::BottomRight) {
                    c.setY(m.screen.bottom() - marker.height() / 2 - 4);
                }
                if (sa == SA::Left || sa == SA::LeftCenter || sa == SA::TopLeft
                    || sa == SA::BottomLeft) {
                    c.setX(m.screen.left() + marker.width() / 2 + 4);
                }
                if (sa == SA::Right || sa == SA::RightCenter || sa == SA::TopRight
                    || sa == SA::BottomRight) {
                    c.setX(m.screen.right() - marker.width() / 2 - 4);
                }
                marker.moveCenter(c);
            }
            const bool selected = m_session.isItemSelected(item.id);
            p.setBrush(QColor(96, 205, 255, selected ? 80 : 40));
            p.setPen(QPen(QColor(QStringLiteral("#60cdff")), selected ? 2.0 : 1.0, Qt::DashLine));
            p.drawRoundedRect(marker, 6, 6);
            p.setPen(Qt::white);
            p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
            p.drawText(marker, Qt::AlignCenter, item.label.isEmpty() ? item.id : item.label);
    }
}

void LayoutEditorCanvas::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r,
                                   bool selected, bool hovered) const
{
    const LayoutDocument& doc = m_session.document();
    const LayoutItemStyle st = doc.style.withOverrides(item.style);
    const double radius = st.radius.value_or(14.0);
    QColor bg = st.background.value_or(m_theme.cellBg);
    QColor fg = st.foreground.value_or(m_theme.text);
    if (hovered && !selected) {
        bg = st.background ? bg.lighter(118) : m_theme.cellHover;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);
    double bw = st.borderWidth.value_or(0.0);
    QColor border = st.borderColor.value_or(m_theme.border);
    if (selected) {
        bw = qMax(bw, 2.0);
        border = QColor(QStringLiteral("#e8a87c"));
    } else if (hovered) {
        bw = qMax(bw, 1.5);
        border = m_theme.accent;
    }
    if (bw > 0.0 && border.alpha() > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, bw));
        p.drawRoundedRect(r, radius, radius);
    }

    p.setPen(fg);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12, r.height() * 0.52);
        MouseIcons::paint(p, item.icon, iconR, fg);
        p.setFont(QFont(QStringLiteral("Segoe UI"), qBound(8, int(r.height() * 0.18), 12),
                        QFont::DemiBold));
        p.drawText(r.adjusted(6, r.height() * 0.52, -6, -4),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
    } else {
        const int px = qBound(8, int(r.height() * 0.28), 14);
        p.setFont(QFont(QStringLiteral("Segoe UI"), px, QFont::DemiBold));
        const QString text = item.caption.isEmpty()
                                 ? (item.label.isEmpty() ? item.id : item.label)
                                 : QStringLiteral("%1\n%2").arg(item.label, item.caption);
        p.drawText(r.adjusted(6, 4, -6, -4), Qt::AlignCenter | Qt::TextWordWrap, text);
    }
}

LayoutEditorCanvas::Drag LayoutEditorCanvas::hitHandle(const QPoint& pos, const ScreenMap& m) const
{
    const QString id = m_session.selection().itemId;
    if (id.isEmpty()) {
        return Drag::None;
    }
    const auto rects =
        LayoutGeometry::itemRects(m_session.document(), int(m.board.width()), int(m.board.height()));
    const QRectF r = rects.value(id).translated(m.board.topLeft());
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
    } else if (m_drag == Drag::Window) {
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
        const auto rects = LayoutGeometry::itemRects(m_session.document(), int(m.board.width()),
                                                     int(m.board.height()));
        m_pressRect = rects.value(m_dragId);
        return;
    }

    const QString id = hitItem(event->pos(), m);
    if (!id.isEmpty()) {
        m_drag = Drag::Move;
        m_dragId = id;
        m_session.selectItem(id, event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
        return;
    }

    if (!hitBoard(event->pos(), m)) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    if (m_session.placeKind()) {
        const QPoint cell = cellAt(event->pos(), m);
        m_session.addItemAt(*m_session.placeKind(), cell.y(), cell.x());
        return;
    }
    if (!m_fitBoard) {
        m_drag = Drag::Window;
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
        if (!m_dragId.isEmpty() && (pos - m_pressPos).manhattanLength() > 8 && m.board.contains(pos)) {
            const QPoint cell = cellAt(pos, m);
            const LayoutItem* it = m_session.itemById(m_dragId);
            const int dRow = it ? cell.y() - it->row : 0;
            const int dCol = it ? cell.x() - it->col : 0;
            if (m_session.selection().itemIds.size() > 1) {
                m_session.moveSelected(dRow, dCol);
            } else {
                m_session.moveItemToCell(m_dragId, cell.y(), cell.x());
            }
        }
        break;
    case Drag::ResizeW:
    case Drag::ResizeH:
        applyResize(pos, m);
        break;
    case Drag::Rubber: {
        const auto rects = LayoutGeometry::itemRects(m_session.document(), int(m.board.width()),
                                                     int(m.board.height()));
        const QRectF band = QRectF(m_rubber.normalized()).translated(-m.board.topLeft());
        QStringList ids;
        for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
            if (it.value().intersects(band)) {
                ids.push_back(it.key());
            }
        }
        m_session.selectItems(ids);
        m_rubber = {};
        break;
    }
    case Drag::Window: {
        const QPointF delta = pos - m_pressPos;
        const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
        QRect board = LayoutGeometry::boardRectInBounds(m_session.document(), virt);
        board.translate(int(delta.x() / qMax(0.001, m.scale)),
                        int(delta.y() / qMax(0.001, m.scale)));
        m_session.snapWindowTo(board.topLeft(), m.virtualScreen);
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
    const LayoutItem* item = id.isEmpty() ? nullptr : m_session.itemById(id);
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
    const LayoutItem* item = m_session.itemById(m_dragId);
    if (!item || m_pressRect.width() < 1) {
        return;
    }
    const QPointF local(pos.x() - m.board.x(), pos.y() - m.board.y());
    if (m_drag == Drag::ResizeW) {
        const double newW = qMax(8.0, local.x() - m_pressRect.left());
        if (m_session.document().grid.unitRows || item->widthUnits > 0) {
            const double u = item->widthUnits > 0 ? item->widthUnits : 1.0;
            m_session.resizeItem(item->id, item->rowSpan, item->colSpan,
                                 qMax(0.3, u * (newW / m_pressRect.width())));
        } else {
            const double cellW = m_pressRect.width() / qMax(1, item->colSpan);
            m_session.resizeItem(item->id, item->rowSpan,
                                 qMax(1, qRound(newW / qMax(8.0, cellW))), item->widthUnits);
        }
    } else {
        const double newH = qMax(8.0, local.y() - m_pressRect.top());
        const double cellH = m_pressRect.height() / qMax(1, item->rowSpan);
        m_session.resizeItem(item->id, qMax(1, qRound(newH / qMax(8.0, cellH))), item->colSpan,
                             item->widthUnits);
    }
}

void LayoutEditorCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        m_session.deleteSelected();
        return;
    }
    int dRow = 0;
    int dCol = 0;
    if (event->key() == Qt::Key_Left) {
        dCol = -1;
    } else if (event->key() == Qt::Key_Right) {
        dCol = 1;
    } else if (event->key() == Qt::Key_Up) {
        dRow = -1;
    } else if (event->key() == Qt::Key_Down) {
        dRow = 1;
    }
    if (dRow || dCol) {
        m_session.moveSelected(dRow, dCol);
        return;
    }
    QWidget::keyPressEvent(event);
}

void LayoutEditorCanvas::leaveEvent(QEvent*)
{
    if (!m_hoverId.isEmpty()) {
        m_hoverId.clear();
        update();
    }
}

} // namespace gazer
