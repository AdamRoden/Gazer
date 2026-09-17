#include "editor/LayoutEditorCanvas.h"

#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "ui/BoardPaint.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPen>

namespace gazer {
namespace {

QColor zoneProgressBorder(const ThemeColors& theme, const PageTarget& t, const QColor& canvas)
{
    QColor c;
    if (const std::optional<QColor> pc = theme.resolveToken(t.chrome.progressColor.token, canvas)) {
        if (pc->isValid()) {
            c = *pc;
        }
    }
    if (!c.isValid()) {
        c = theme.progress.isValid() ? theme.progress : ThemeColors::defaultProgressColor();
    }
    c.setAlpha(255);
    return c;
}

} // namespace

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
    const bool dark = m_theme.bgMain.lightness() < 128;
    p.setPen(Qt::NoPen);
    for (int i = 10; i >= 1; --i) {
        QColor shadow = dark ? QColor(0, 0, 0, 12) : QColor(20, 24, 32, 10);
        p.setBrush(shadow);
        p.drawRoundedRect(m.bezel.adjusted(-i, -i + 3, i, i + 5), 12 + i * 0.4, 12 + i * 0.4);
    }
    const QColor bezel = dark ? m_theme.defaultCell().darker(135) : m_theme.border;
    const QColor glass = dark ? m_theme.bgMain.darker(110) : m_theme.defaultActive();
    p.setBrush(bezel);
    p.drawRoundedRect(m.bezel, 10, 10);
    p.setBrush(glass);
    p.drawRoundedRect(m.glass, 6, 6);

    QLinearGradient desk(m.screen.topLeft(), m.screen.bottomLeft());
    desk.setColorAt(0, m_theme.bgAt(90));
    desk.setColorAt(1, m_theme.bgAt(95));
    p.setBrush(desk);
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
        p.setBrush(m_theme.bgAt(80));
        p.drawRect(bar);

        const bool horizontal = bar.width() >= bar.height();
        QColor edge = m_theme.border;
        edge.setAlpha(160);
        p.setPen(QPen(edge, 1));
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
            QColor idle = m_theme.text;
            idle.setAlpha(40 + i * 10);
            p.setBrush(i == 0 ? m_theme.accent : idle);
            p.drawRoundedRect(r, qMin(3.0, icon * 0.28), qMin(3.0, icon * 0.28));
        }
    }
    p.restore();
}

void LayoutEditorCanvas::paintBoard(QPainter& p, const ScreenMap& m) const
{
    const QColor canvas = m_theme.pageCanvas(m_session.document().style.background.token);

    // Dwell wash in widget space, before chrome, so chips cover the overlap.
    for (const PageTarget& t : m.targets) {
        if (t.kind != PageTarget::Kind::Zone) {
            continue;
        }
        const QRectF progress = toCanvas(targetRect(t), m);
        const QRectF dwell = toCanvas(t.geom.dwellZone, m);
        if (dwell.isEmpty() || dwell == progress) {
            continue;
        }
        QColor fill = zoneProgressBorder(m_theme, t, canvas);
        fill.setAlpha(36);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRect(dwell);
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(m.screen.intersected(QRectF(rect())));
    p.translate(m.screen.topLeft());
    p.scale(m.scaleX, m.scaleY);

    if (m_showGrid && !m.boardVirt.isEmpty()) {
        QColor gridLine = m_theme.border;
        gridLine.setAlpha(90);
        QPen gp(gridLine, 1);
        gp.setCosmetic(true);
        p.setPen(gp);
        const PageGrid* g = PageEdit::primaryGrid(m_session.document());
        const int cols = g ? qMax(1, g->columns) : 4;
        const int rows = g ? qMax(1, g->rows) : 2;
        for (int c = 1; c < cols; ++c) {
            const double x = m.boardVirt.left() + m.boardVirt.width() * c / cols;
            p.drawLine(QPointF(x, m.boardVirt.top()), QPointF(x, m.boardVirt.bottom()));
        }
        for (int r = 1; r < rows; ++r) {
            const double y = m.boardVirt.top() + m.boardVirt.height() * r / rows;
            p.drawLine(QPointF(m.boardVirt.left(), y), QPointF(m.boardVirt.right(), y));
        }
    }
    if (m_session.selection().target == EditorTarget::Grid) {
        const QString gid = m_session.selection().itemId;
        for (const PageGridPaint& g : m.grids) {
            if (g.gridId != gid) {
                continue;
            }
            QColor fill = m_theme.accent;
            fill.setAlpha(18);
            p.setBrush(fill);
            QPen dash(m_theme.accent, 1.5, Qt::DashLine);
            dash.setCosmetic(true);
            p.setPen(dash);
            p.drawRoundedRect(g.visual.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        }
    }
    for (const PageGridPaint& g : m.grids) {
        BoardPaint::paintSurface(p, g.visual, g.chrome, m_theme, nullptr, true, false, false,
                                 false, canvas);
    }
    for (const PageTarget& t : m.targets) {
        const QRectF r = targetRect(t);
        if (r.isEmpty()) {
            continue;
        }
        const bool hovered = t.id == m_hoverId;
        const bool selected = m_session.isItemSelected(t.id);
        BoardPaint::paintTarget(p, t, r, m_theme, canvas, nullptr, hovered,
                                hovered ? m_testProgress : 0.0, false, selected, {});
    }
    p.restore();

    auto hoverRing = [&](const QRectF& canvasR) {
        QColor hover = m_theme.accent;
        hover.setAlpha(140);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(hover, 1.4));
        p.drawRoundedRect(canvasR.adjusted(1, 1, -1, -1), 6, 6);
    };
    for (const PageTarget& t : m.targets) {
        const bool hovered = t.id == m_hoverId;
        const bool selected = m_session.isItemSelected(t.id);
        const QRectF chrome = toCanvas(targetRect(t), m);
        if (t.kind == PageTarget::Kind::Zone) {
            const QRectF dwell = toCanvas(t.geom.dwellZone, m);
            if (!dwell.isEmpty() && dwell != chrome) {
                const QColor border = zoneProgressBorder(m_theme, t, canvas);
                p.setBrush(Qt::NoBrush);
                const QPolygonF virtHull = PageHit::accumulatePolygon(t);
                if (virtHull.size() >= 3) {
                    QPolygonF hull;
                    hull.reserve(virtHull.size());
                    for (const QPointF& pt : virtHull) {
                        hull << m.fromVirt(pt);
                    }
                    p.setPen(QPen(border, 1.2, Qt::DotLine));
                    p.drawPolygon(hull);
                }
                p.setPen(QPen(border, selected ? 2.2 : 1.6));
                p.drawRect(dwell);
            }
            if (hovered && !selected && !chrome.isEmpty()) {
                hoverRing(chrome);
            }
            if (selected && !chrome.isEmpty()) {
                paintSelectionOverlay(p, chrome, true);
            }
            continue;
        }
        if (hovered && !selected) {
            hoverRing(chrome);
        }
        if (selected) {
            paintSelectionOverlay(p, chrome, true);
        }
    }
}

void LayoutEditorCanvas::paintSelectionOverlay(QPainter& p, const QRectF& r, bool handles) const
{
    QColor wash = m_theme.accent;
    wash.setAlpha(22);
    p.setBrush(wash);
    p.setPen(QPen(m_theme.accent, 1.8));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 6, 6);
    if (handles) {
        paintHandles(p, r);
    }
}

void LayoutEditorCanvas::paintHandles(QPainter& p, const QRectF& r) const
{
    auto handle = [&](const QPointF& c) {
        const QRectF box(c.x() - 5, c.y() - 5, 10, 10);
        p.setBrush(m_theme.defaultCell());
        p.setPen(QPen(m_theme.accent, 1.4));
        p.drawRoundedRect(box, 2, 2);
    };
    handle(r.topLeft());
    handle(QPointF(r.center().x(), r.top()));
    handle(r.topRight());
    handle(QPointF(r.right(), r.center().y()));
    handle(r.bottomRight());
    handle(QPointF(r.center().x(), r.bottom()));
    handle(r.bottomLeft());
    handle(QPointF(r.left(), r.center().y()));
}

void LayoutEditorCanvas::paintGuides(QPainter& p, const ScreenMap& m) const
{
    if (!m_ghostVirt.isEmpty()) {
        const QRectF r = toCanvas(m_ghostVirt, m);
        QColor fill = m_theme.accent;
        fill.setAlpha(40);
        p.setBrush(fill);
        p.setPen(QPen(m_theme.accent, 1.4, Qt::DashLine));
        p.drawRoundedRect(r, 6, 6);
    }
    if (m_guides.isEmpty()) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    QPen pen(m_theme.accent, 1, Qt::DashLine);
    p.setPen(pen);
    for (const QLineF& line : m_guides) {
        p.drawLine(QLineF(m.fromVirt(line.p1()), m.fromVirt(line.p2())));
    }
}

} // namespace gazer
