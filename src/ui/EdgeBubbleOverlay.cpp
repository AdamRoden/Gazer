#include "ui/EdgeBubbleOverlay.h"

#include "utils/WinOverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QtMath>

namespace gazer {

namespace {

/// Progress fill: starts at the screen edge and grows (offset) until the bubble border.
/// Intersection of the half-ellipse shape with a depth strip from the edge.
QPainterPath edgeOffsetFill(const DwellRegionSpace::EdgeBand& band, double progress,
                            const QPoint& origin)
{
    const qreal t = qBound(0.0, progress, 1.0);
    if (t <= 0.001) {
        return {};
    }

    const QRectF full = band.fullEllipse.translated(-origin);
    const QRectF on = band.onScreen.translated(-origin);
    if (on.isEmpty() || full.isEmpty()) {
        return {};
    }

    QPainterPath shape;
    shape.addEllipse(full);
    QPainterPath clip;
    clip.addRect(on);
    shape = shape.intersected(clip);

    // Strip from screen edge → interior by fraction t of depth.
    // ∩ ellipse ⇒ leading edge is a smooth curve that expands to the border.
    const QRectF strip = DwellRegionSpace::progressStrip(band, t).translated(-origin);
    if (strip.isEmpty()) {
        return {};
    }
    QPainterPath grow;
    grow.addRect(strip);
    return shape.intersected(grow);
}

} // namespace

EdgeBubbleOverlay::EdgeBubbleOverlay(QObject* parent)
    : OverlaySurface(nullptr)
{
    Q_UNUSED(parent);
    hide();
    connectScreenSignals();
    syncGeometry();
}

void EdgeBubbleOverlay::connectScreenSignals()
{
    auto resync = [this]() {
        syncGeometry();
        update();
    };
    if (auto* app = qGuiApp) {
        connect(app, &QGuiApplication::primaryScreenChanged, this, [resync](QScreen*) { resync(); });
        connect(app, &QGuiApplication::screenAdded, this, [this, resync](QScreen* s) {
            if (s) {
                connect(s, &QScreen::geometryChanged, this, [resync](const QRect&) { resync(); });
            }
            resync();
        });
        connect(app, &QGuiApplication::screenRemoved, this, [resync](QScreen*) { resync(); });
        for (QScreen* s : app->screens()) {
            if (s) {
                connect(s, &QScreen::geometryChanged, this, [resync](const QRect&) { resync(); });
            }
        }
    }
}

void EdgeBubbleOverlay::syncGeometry()
{
    const QRect unionRect = DwellRegionSpace::virtualDesktop();
    if (!unionRect.isEmpty()) {
        setGeometry(unionRect);
    }
}

void EdgeBubbleOverlay::setBubble(const Bubble& bubble)
{
    m_bubbles.insert(bubble.key, bubble);
    syncGeometry();
    if (!isVisible()) {
        showOverlay();
    } else {
        raise();
        applyOverlayWindowChrome(this, /*excludeFromCapture=*/true);
    }
    update();
}

void EdgeBubbleOverlay::clearBubble(const QString& key)
{
    if (auto* t = m_flashTimers.take(key)) {
        t->stop();
        t->deleteLater();
    }
    if (m_bubbles.remove(key) > 0) {
        if (m_bubbles.isEmpty()) {
            hide();
        } else {
            update();
        }
    }
}

void EdgeBubbleOverlay::clearAll()
{
    const auto keys = m_flashTimers.keys();
    for (const QString& k : keys) {
        if (auto* t = m_flashTimers.take(k)) {
            t->stop();
            t->deleteLater();
        }
    }
    m_bubbles.clear();
    hide();
}

void EdgeBubbleOverlay::flashThenClear(const QString& key, const ProgressVisuals& visuals,
                                       int flashMs)
{
    Bubble b = m_bubbles.value(key);
    b.key = key;
    b.visuals = visuals;
    b.flashing = true;
    b.progress = 1.0;
    if (b.band.onScreen.isEmpty() && m_bubbles.contains(key)) {
        b.band = m_bubbles.value(key).band;
        b.label = m_bubbles.value(key).label;
    }
    setBubble(b);

    if (auto* old = m_flashTimers.take(key)) {
        old->stop();
        old->deleteLater();
    }
    auto* t = new QTimer(this);
    t->setSingleShot(true);
    // clearBubble owns timer cleanup (take + deleteLater).
    connect(t, &QTimer::timeout, this, [this, key]() { clearBubble(key); });
    m_flashTimers.insert(key, t);
    t->start(qMax(40, flashMs));
}

void EdgeBubbleOverlay::showEvent(QShowEvent* event)
{
    OverlaySurface::showEvent(event);
    syncGeometry();
}

void EdgeBubbleOverlay::paintEvent(QPaintEvent* /*event*/)
{
    if (m_bubbles.isEmpty()) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPoint origin = geometry().topLeft();

    for (const Bubble& b : m_bubbles) {
        const auto& band = b.band;
        if (band.onScreen.isEmpty()) {
            continue;
        }

        const QRectF full = band.fullEllipse.translated(-origin);
        const QRectF visible = band.onScreen.translated(-origin);

        QPainterPath shape;
        shape.addEllipse(full);
        QPainterPath clip;
        clip.addRect(visible);
        shape = shape.intersected(clip);

        p.save();

        const ProgressVisuals& v = b.visuals;
        const QColor base = v.progressColor.isValid() ? v.progressColor : QColor(0, 220, 255);

        // Dim track of full affordance
        QColor track = base;
        track.setAlpha(36);
        p.setPen(QPen(QColor(base.red(), base.green(), base.blue(), 80), 2.0));
        p.setBrush(track);
        p.drawPath(shape);

        if (b.flashing && v.flashOnComplete) {
            p.setPen(QPen(v.flashBorderColor, 3.5));
            p.setBrush(v.flashFillColor);
            p.drawPath(shape);
            if (!b.label.isEmpty()) {
                p.setPen(QColor(240, 248, 255));
                p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
                p.drawText(visible.toRect().adjusted(4, 4, -4, -4),
                           Qt::AlignCenter | Qt::TextWordWrap, b.label);
            }
            p.restore();
            continue;
        }

        const double prog = qBound(0.0, b.progress, 1.0);
        if (prog > 0.005) {
            // From screen edge → offset fill until the bubble border (never a center clock ring).
            const QPainterPath fillPath = edgeOffsetFill(band, prog, origin);
            if (!fillPath.isEmpty()) {
                QColor fill = v.fillColor.isValid() ? v.fillColor : base;
                fill.setAlpha(qBound(50, int(80 + 140 * prog), 230));
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawPath(fillPath);

                // Emphasize the leading offset curve (outer path of the fill)
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(base, 2.2 + prog * 1.5));
                p.drawPath(fillPath);
            }

            if (v.border) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(v.borderColor.isValid() ? v.borderColor : base, 2.0 + 2.0 * prog));
                p.drawPath(shape);
            }
        }

        // Quiet outer rim at full border
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(base.red(), base.green(), base.blue(), 130), 2.0));
        p.drawPath(shape);

        if (!b.label.isEmpty()) {
            p.setPen(QColor(240, 248, 255));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
            p.drawText(visible.toRect().adjusted(4, 4, -4, -4),
                       Qt::AlignCenter | Qt::TextWordWrap, b.label);
        }
        p.restore();
    }
}

} // namespace gazer
