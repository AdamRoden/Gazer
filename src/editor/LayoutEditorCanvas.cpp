#include "editor/LayoutEditorCanvas.h"

#include "layout/LayoutGeometry.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace gazer {

LayoutEditorCanvas::LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMinimumSize(480, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    connect(&m_session, &LayoutEditorSession::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::selectionChanged, this, QOverload<>::of(&QWidget::update));
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

QString LayoutEditorCanvas::hitItem(const QPoint& pos) const
{
    const ScreenMap m = map();
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

bool LayoutEditorCanvas::hitBoard(const QPoint& pos) const
{
    return map().board.contains(pos);
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
        const bool selected = sel.target == EditorTarget::Item && sel.itemId == item.id;
        const bool hovered = item.id == m_hoverId;
        paintCell(p, item, r, selected, hovered);
    }
    p.restore();

    // Unbounded markers on the virtual screen (monitor view).
    if (!m_fitBoard) {
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
            const bool selected = sel.target == EditorTarget::Item && sel.itemId == item.id;
            p.setBrush(QColor(96, 205, 255, selected ? 80 : 40));
            p.setPen(QPen(QColor(QStringLiteral("#60cdff")), selected ? 2.0 : 1.0, Qt::DashLine));
            p.drawRoundedRect(marker, 6, 6);
            p.setPen(Qt::white);
            p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
            p.drawText(marker, Qt::AlignCenter, item.label.isEmpty() ? item.id : item.label);
        }
    }
}

void LayoutEditorCanvas::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r,
                                   bool selected, bool hovered) const
{
    const LayoutDocument& doc = m_session.document();
    const LayoutItemStyle st = doc.style.withOverrides(item.style);
    const double radius = st.radius.value_or(14.0);
    QColor bg = st.background.value_or(QColor(QStringLiteral("#f4f6f8")));
    QColor fg = st.foreground.value_or(QColor(QStringLiteral("#1b1d21")));
    if (!st.background) {
        bg = QColor(245, 247, 250);
        fg = QColor(28, 30, 34);
    }
    if (hovered && !selected) {
        bg = bg.lighter(108);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);
    const double bw = st.borderWidth.value_or(0.0);
    if (bw > 0.0 && st.borderColor) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(*st.borderColor, bw));
        p.drawRoundedRect(r, radius, radius);
    }
    if (selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(QStringLiteral("#e8a87c")), 2.4));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
    } else if (hovered) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 180, 190, 160), 2.0));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
    }

    p.setPen(fg);
    const int px = qBound(8, int(r.height() * 0.32), 16);
    p.setFont(QFont(QStringLiteral("Segoe UI"), px, QFont::DemiBold));
    const QString text = item.label.isEmpty() ? item.id : item.label;
    p.drawText(r.adjusted(4, 3, -4, -3), Qt::AlignCenter | Qt::TextWordWrap, text);
}

void LayoutEditorCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_pressPos = event->pos();
    m_dragging = false;
    const QString id = hitItem(event->pos());
    if (!id.isEmpty()) {
        m_dragId = id;
        m_session.selectItem(id);
        return;
    }
    m_dragId.clear();
    if (hitBoard(event->pos())) {
        m_session.selectTarget(EditorTarget::Window);
    } else {
        m_session.selectTarget(EditorTarget::Document);
    }
}

void LayoutEditorCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QString hover = hitItem(event->pos());
    if (hover != m_hoverId) {
        m_hoverId = hover;
        update();
    }
    if (!(event->buttons() & Qt::LeftButton) || m_dragId.isEmpty()) {
        return;
    }
    if ((event->pos() - m_pressPos).manhattanLength() > 8) {
        m_dragging = true;
    }
}

void LayoutEditorCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (m_dragging && !m_dragId.isEmpty()) {
        const ScreenMap m = map();
        if (m.board.contains(event->pos())) {
            const QPoint cell = cellAt(event->pos(), m);
            m_session.moveItemToCell(m_dragId, cell.y(), cell.x());
        }
    }
    m_dragging = false;
    m_dragId.clear();
}

void LayoutEditorCanvas::leaveEvent(QEvent*)
{
    if (!m_hoverId.isEmpty()) {
        m_hoverId.clear();
        update();
    }
}

} // namespace gazer
