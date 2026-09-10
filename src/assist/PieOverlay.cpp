#include "assist/PieOverlay.h"

#include "ui/KeySymbols.h"
#include "ui/ProgressVisuals.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QtMath>

namespace gazer {
namespace {

[[nodiscard]] QPainterPath wedgePath(const QPointF& c, const ComboMouseHit::Wedge& w)
{
    const double qtStart = 90.0 - w.startCw;
    QPainterPath slice;
    slice.moveTo(c);
    slice.arcTo(QRectF(c.x() - w.outer, c.y() - w.outer, w.outer * 2.0, w.outer * 2.0), qtStart,
                -w.spanDeg);
    slice.closeSubpath();
    QPainterPath hole;
    hole.addEllipse(c, w.inner, w.inner);
    return slice.subtracted(hole);
}

void paintPathProgress(QPainter& p, const QPainterPath& zone, const QPointF& c, double outerR,
                       double startDeg, double spanDeg, double progress, const ProgressVisuals& vis)
{
    if (progress <= 0.0 || zone.isEmpty()) {
        return;
    }
    if (vis.style.fillBackground) {
        QColor fill = vis.fillColor.isValid() ? vis.fillColor : vis.progressColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 24 * progress), 220));
        p.save();
        p.setClipPath(zone);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawEllipse(c, outerR * progress, outerR * progress);
        p.restore();
    }
    if (vis.style.border) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(vis.borderColor, 2.0 + 2.4 * progress, Qt::SolidLine, Qt::FlatCap,
                      Qt::RoundJoin));
        p.drawPath(zone);
    }
    if (vis.style.radial) {
        const qreal penW = 5.0;
        const QRectF arc(c.x() - outerR + penW, c.y() - outerR + penW, (outerR - penW) * 2.0,
                         (outerR - penW) * 2.0);
        p.setBrush(Qt::NoBrush);
        QColor track = vis.progressColor;
        track.setAlpha(70);
        p.setPen(QPen(track, penW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int(startDeg * 16.0), int(spanDeg * 16.0));
        p.setPen(QPen(vis.progressColor, penW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int(startDeg * 16.0), int(spanDeg * 16.0 * progress));
    }
}

} // namespace

PieOverlay::PieOverlay()
{
    hide();
}

void PieOverlay::setAppearance(const Appearance& a)
{
    m_a = a;
    m_a.dwellProg = qBound(0.0, a.dwellProg, 1.0);
    if (!m_a.accent.isValid()) {
        m_a.accent = ThemeColors::defaultProgressColor();
    }
    if (!m_a.innerColor.isValid()) {
        m_a.innerColor = ComboMouseHit::kDefaultInnerFill;
    }
    if (!m_a.outerColor.isValid()) {
        m_a.outerColor = ComboMouseHit::kDefaultOuterFill;
    }
    update();
}

void PieOverlay::place(const QPoint& origin, const QRectF& screen)
{
    const QRect wr = ComboMouseHit::overlayRect(QPointF(origin), m_a.layout, screen);
    m_originLocal = QPointF(origin) - QPointF(wr.topLeft());
    if (wr.size() != size()) {
        resize(wr.size());
    }
    if (pos() != wr.topLeft()) {
        move(wr.topLeft());
    }
    showOverlay();
    update();
}

void PieOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF c = m_originLocal;
    const QColor cyan = m_a.accent.isValid() ? m_a.accent : ThemeColors::defaultProgressColor();
    paintCommands(p, c, cyan);
    paintInnerRing(p, c, cyan);
    paintHubLabel(p, c);
}

void PieOverlay::paintCommands(QPainter& p, const QPointF& c, const QColor& cyan)
{
    for (int i = 0; i < m_a.layout.wedgeCount; ++i) {
        paintWedge(p, c, cyan, m_a.layout.wedges[i]);
    }
}

void PieOverlay::paintWedge(QPainter& p, const QPointF& c, const QColor& cyan,
                            const ComboMouseHit::Wedge& w)
{
    const bool hover = m_a.band == ComboMouseHit::Band::Slice && m_a.slice == w.id;
    const bool armed = m_a.armed && w.id == m_a.armedSlice;
    const QPainterPath zone = wedgePath(c, w);

    QColor fill = m_a.outerColor;
    if (hover || armed) {
        fill.setAlpha(qBound(0, fill.alpha() + 20, 255));
    }
    if (armed && m_a.theme.cellActive.isValid()) {
        fill = m_a.theme.cellActive;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(zone);

    const QColor primary = m_a.theme.accent.isValid() ? m_a.theme.accent : cyan;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(armed && m_a.theme.accentHover.isValid() ? m_a.theme.accentHover : primary,
                  hover || armed ? 3.0 : 2.0, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    p.drawPath(zone);

    ProgressVisuals vis;
    vis.style.radial = true;
    vis.style.fillBackground = true;
    vis.fillColor = QColor(cyan.red(), cyan.green(), cyan.blue(), 90);
    vis.progressColor = cyan;
    vis.borderColor = primary;
    const double qtStart = 90.0 - w.startCw;
    const double prog = hover ? qMax(0.08, m_a.dwellProg) : 0.0;
    paintPathProgress(p, zone, c, w.outer, qtStart, -w.spanDeg, prog, vis);

    const double midDeg = w.startCw + w.spanDeg * 0.5;
    const double rad = qDegreesToRadians(midDeg - 90.0);
    const double midR = (w.inner + w.outer) * 0.5;
    const QPointF mid(c.x() + qCos(rad) * midR, c.y() + qSin(rad) * midR);
    const double iconSide = qBound(40.0, (w.outer - w.inner) * 0.78, 108.0);
    const QRectF icon(mid.x() - iconSide * 0.5, mid.y() - iconSide * 0.5, iconSide, iconSide);
    const QColor fg =
        armed && m_a.theme.accent.isValid() ? m_a.theme.accent : QColor(255, 255, 255, 235);
    const int idx = int(w.id);
    const char* name = (idx >= 0 && idx < ComboMouseHit::kSliceCount) ? m_a.sliceIcons[idx]
                                                                     : nullptr;
    if (name && name[0]) {
        KeySymbols::paint(p, QLatin1String(name), icon, fg);
    }
}

void PieOverlay::paintInnerRing(QPainter& p, const QPointF& c, const QColor& cyan)
{
    const double inner = m_a.layout.deadzone;
    const double outer = m_a.layout.ringOuter;
    QPainterPath ring;
    ring.addEllipse(c, outer, outer);
    QPainterPath hole;
    hole.addEllipse(c, inner, inner);
    const QPainterPath annulus = ring.subtracted(hole);
    QColor fill = m_a.innerColor;
    if (m_a.innerActive) {
        fill.setAlpha(qBound(0, fill.alpha() + 40, 255));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(annulus);
    ProgressVisuals vis;
    vis.style.radial = true;
    vis.style.fillBackground = true;
    vis.style.border = true;
    vis.borderColor = cyan;
    vis.fillColor = QColor(cyan.red(), cyan.green(), cyan.blue(), 70);
    vis.progressColor = cyan;
    paintPathProgress(p, annulus, c, outer, 90.0, -360.0, m_a.innerActive ? m_a.dwellProg : 0.0,
                      vis);

    const double len = qSqrt(m_a.driftDir.x() * m_a.driftDir.x() + m_a.driftDir.y() * m_a.driftDir.y());
    if (len > 0.05) {
        const QPointF tip = c + m_a.driftDir / len * ((inner + outer) * 0.5);
        p.setPen(QPen(QColor(cyan.red(), cyan.green(), cyan.blue(), 220), 2.4, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawLine(c, tip);
    }
}

void PieOverlay::paintHubLabel(QPainter& p, const QPointF& c)
{
    if (m_a.hubLabel.isEmpty()) {
        return;
    }
    const double inner = m_a.layout.deadzone;
    QFont f = p.font();
    f.setPixelSize(int(qBound(16.0, inner * 0.55, 36.0)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 235));
    const QRectF box(c.x() - inner, c.y() - inner, inner * 2.0, inner * 2.0);
    p.drawText(box, Qt::AlignCenter, m_a.hubLabel);
}

} // namespace gazer
