#include "ui/EdgeBubbleOverlay.h"

#include "utils/WinOverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>

namespace gazer {

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
    m_bubbles.clear();
    hide();
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

        const QRectF ellipse = band.fullEllipse.translated(-origin);
        const QRectF visible = band.onScreen.translated(-origin);

        QPainterPath ellipsePath;
        ellipsePath.addEllipse(ellipse);
        QPainterPath visibleClip;
        visibleClip.addRect(visible);
        const QPainterPath shape = ellipsePath.intersected(visibleClip);

        p.save();

        QColor track = b.color;
        track.setAlpha(40);
        p.setPen(QPen(QColor(b.color.red(), b.color.green(), b.color.blue(), 90), 2.0));
        p.setBrush(track);
        p.drawPath(shape);

        const double prog = qBound(0.0, b.progress, 1.0);
        if (prog > 0.005) {
            const QRectF strip =
                DwellRegionSpace::progressStrip(band, prog).translated(-origin);
            QPainterPath fillPath;
            fillPath.addRect(strip);
            fillPath = fillPath.intersected(shape);

            QColor fill = b.color;
            fill.setAlpha(90 + int(120 * prog));
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawPath(fillPath);

            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(b.color.red(), b.color.green(), b.color.blue(), 200), 2.5));
            p.drawPath(fillPath);
        }

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(b.color.red(), b.color.green(), b.color.blue(), 160), 2.5));
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
