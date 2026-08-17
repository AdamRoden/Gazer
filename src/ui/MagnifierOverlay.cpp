#include "ui/MagnifierOverlay.h"

#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QtMath>

namespace gazer {

MagnifierOverlay::MagnifierOverlay(QWidget* parent)
    : OverlaySurface(parent)
{
    resize(m_lensSize, m_lensSize);
    hide();

    m_captureTimer.setInterval(100);
    connect(&m_captureTimer, &QTimer::timeout, this, [this]() {
        if (m_enabled && m_pendingCenterValid) {
            refreshCapture(m_pendingCenter);
            update();
        }
    });
}

void MagnifierOverlay::setEnabledLens(bool enabled)
{
    m_enabled = enabled;
    m_smoothValid = false;
    if (m_enabled) {
        showOverlay();
        m_captureTimer.start();
    } else {
        m_captureTimer.stop();
        m_capture = {};
        m_pendingCenterValid = false;
        hide();
    }
    GAZER_INFO << "Magnifier" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
}

void MagnifierOverlay::toggle()
{
    setEnabledLens(!m_enabled);
}

void MagnifierOverlay::setZoom(double factor)
{
    m_zoom = qBound(1.25, factor, 6.0);
    m_sourceRadius = qMax(20, static_cast<int>(m_lensSize / (2.0 * m_zoom)));
}

void MagnifierOverlay::setLensSize(int px)
{
    m_lensSize = qBound(120, px, 900);
    resize(m_lensSize, m_lensSize);
    m_sourceRadius = qMax(20, static_cast<int>(m_lensSize / (2.0 * m_zoom)));
}

void MagnifierOverlay::setFollowProfile(int profile)
{
    m_followProfile = qBound(0, profile, 2);
    m_stickiness = GazeFollowStickiness::fromProfile(m_followProfile);
}

void MagnifierOverlay::setAccent(const QColor& c)
{
    if (c.isValid()) {
        m_accent = c;
        update();
    }
}

void MagnifierOverlay::onGaze(const GazePoint& point)
{
    if (!m_enabled || !point.valid) {
        return;
    }

    const QPointF raw(point.x, point.y);
    m_stickiness.smoothPoint(m_smoothCenter, m_smoothValid, raw);

    const QPoint c(qRound(m_smoothCenter.x()), qRound(m_smoothCenter.y()));
    m_pendingCenter = c;
    m_pendingCenterValid = true;

    if (m_lastPlaced.isNull()
        || QLineF(QPointF(m_lastPlaced), QPointF(c)).length() >= m_placeEpsilonPx) {
        m_lastPlaced = c;
        reposition(c);
    }
}

void MagnifierOverlay::refreshCapture(const QPoint& screenCenter)
{
    QScreen* screen = QGuiApplication::screenAt(screenCenter);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const int side = m_sourceRadius * 2;
    const QRect srcGlobal(screenCenter.x() - m_sourceRadius, screenCenter.y() - m_sourceRadius,
                          side, side);
    const QPixmap shot = grabScreenRect(screen, srcGlobal, QColor(12, 14, 18));
    if (shot.isNull()) {
        return;
    }
    m_capture = shot.scaled(m_lensSize, m_lensSize, Qt::IgnoreAspectRatio,
                            Qt::SmoothTransformation);
    m_capture.setDevicePixelRatio(1.0);
}

void MagnifierOverlay::reposition(const QPoint& screenCenter)
{
    move(screenCenter.x() - m_lensSize / 2, screenCenter.y() - m_lensSize / 2);
    raiseStack();
}

void MagnifierOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip;
    clip.addEllipse(rect().adjusted(2, 2, -2, -2));
    p.setClipPath(clip);

    if (!m_capture.isNull()) {
        p.drawPixmap(rect(), m_capture);
    } else {
        p.fillRect(rect(), QColor(20, 24, 30, 200));
    }

    p.setClipping(false);
    p.setPen(QPen(m_accent, 3.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(rect().adjusted(2, 2, -2, -2));

    const QPoint c = rect().center();
    QColor hair = m_accent;
    hair.setAlpha(180);
    p.setPen(QPen(hair, 1.5));
    p.drawLine(c.x() - 10, c.y(), c.x() + 10, c.y());
    p.drawLine(c.x(), c.y() - 10, c.x(), c.y() + 10);
}

} // namespace gazer
