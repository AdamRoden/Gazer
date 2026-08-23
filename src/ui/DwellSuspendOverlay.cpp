#include "ui/DwellSuspendOverlay.h"

#include "utils/ScreenGrab.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>

namespace gazer {

DwellSuspendOverlay::DwellSuspendOverlay(QWidget* parent)
    : OverlaySurface(parent)
{
    hide();
}

void DwellSuspendOverlay::setSuspended(bool suspended)
{
    if (m_suspended == suspended) {
        if (suspended) {
            refreshGeometry();
            update();
        }
        return;
    }
    m_suspended = suspended;
    if (m_suspended) {
        refreshGeometry();
        showOverlay();
        update();
    } else {
        hide();
    }
}

void DwellSuspendOverlay::setGapRects(const QVector<QRect>& gaps)
{
    m_gaps = gaps;
    if (m_suspended) {
        update();
    }
}

void DwellSuspendOverlay::refreshGeometry()
{
    const QRect desk = virtualDesktop();
    if (desk.isValid()) {
        setGeometry(desk);
    }
}

void DwellSuspendOverlay::paintEvent(QPaintEvent*)
{
    if (!m_suspended) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const int b = m_borderPx;
    // Light amber frame — low alpha so the desktop stays readable.
    const QColor border(255, 170, 60, 55);

    // Four edge strips, then punch gaps via clip exclusion.
    QRegion borderRegion;
    borderRegion += QRect(r.left(), r.top(), r.width(), b);
    borderRegion += QRect(r.left(), r.bottom() - b + 1, r.width(), b);
    borderRegion += QRect(r.left(), r.top(), b, r.height());
    borderRegion += QRect(r.right() - b + 1, r.top(), b, r.height());

    for (const QRect& g : m_gaps) {
        // Map screen gap into overlay-local coords.
        const QRect local = g.translated(-pos());
        // Expand gap slightly so the break is visible.
        const QRect expanded = local.adjusted(-12, -12, 12, 12);
        borderRegion -= QRegion(expanded);
    }

    p.setClipRegion(borderRegion);
    p.fillRect(r, border);
    p.setClipping(false);

    // Soft hint text near top center (not over gap).
    p.setPen(QColor(255, 200, 120, 120));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
    p.drawText(QRect(0, b + 4, width(), 24), Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("Dwell paused — look at the break to resume"));
}

} // namespace gazer
