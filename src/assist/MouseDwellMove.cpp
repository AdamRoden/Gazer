#include "assist/MouseDwellMove.h"

#include "input/MouseInjector.h"
#include "utils/Log.h"
#include "utils/WinOverlay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace gazer {

class MouseDwellMove::ReticleOverlay final : public QWidget {
public:
    ReticleOverlay()
        : QWidget(nullptr)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                       | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        resize(80, 80);
        hide();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
        update();
    }

    void setVisuals(const ProgressVisuals& v)
    {
        m_visuals = v;
        update();
    }

    void place(const QPoint& screenCenter)
    {
        move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
        if (!isVisible()) {
            show();
            applyOverlayWindowChrome(this);
        }
    }

protected:
    void showEvent(QShowEvent* e) override
    {
        QWidget::showEvent(e);
        applyOverlayWindowChrome(this);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRect r = rect().adjusted(6, 6, -6, -6);

        if (m_visuals.fillBackground && m_progress > 0.0) {
            QColor fill = m_visuals.fillColor;
            fill.setAlpha(qBound(0, int(fill.alpha() * m_progress + 20), 255));
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawEllipse(r);
        } else {
            p.setPen(QPen(m_visuals.borderColor, 2.0));
            p.setBrush(QColor(m_visuals.fillColor.red(), m_visuals.fillColor.green(),
                              m_visuals.fillColor.blue(), 40));
            p.drawEllipse(r);
        }

        if (m_visuals.border && m_progress > 0.0) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(m_visuals.borderColor, 2.0 + 2.0 * m_progress));
            p.drawEllipse(r);
        }

        if (m_visuals.radial) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(m_visuals.progressColor, 3.5));
            const int span = int(-360 * 16 * m_progress);
            p.drawArc(r, 90 * 16, span);
        }
    }

private:
    double m_progress = 0.0;
    ProgressVisuals m_visuals;
};

MouseDwellMove::MouseDwellMove(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_reticle = std::make_unique<ReticleOverlay>();
}

MouseDwellMove::~MouseDwellMove() = default;

void MouseDwellMove::setArmed(bool armed)
{
    if (m_armed == armed) {
        return;
    }
    m_armed = armed;
    resetDwell();
    if (!m_armed && m_reticle) {
        m_reticle->hide();
    }
    GAZER_INFO << "MouseDwellMove" << (m_armed ? "ARMED" : "off");
    emit armedChanged(m_armed);
}

void MouseDwellMove::toggle()
{
    setArmed(!m_armed);
}

void MouseDwellMove::setDwellMs(int ms)
{
    m_dwellMs = qMax(200, ms);
}

void MouseDwellMove::setStableRadiusPx(int px)
{
    m_stableRadiusPx = qMax(8, px);
    if (m_freezeRadiusPx < m_stableRadiusPx) {
        m_freezeRadiusPx = m_stableRadiusPx;
    }
    if (m_cancelRadiusPx < m_freezeRadiusPx) {
        m_cancelRadiusPx = m_freezeRadiusPx;
    }
}

void MouseDwellMove::setFreezeRadiusPx(int px)
{
    m_freezeRadiusPx = qMax(m_stableRadiusPx, px);
    if (m_cancelRadiusPx < m_freezeRadiusPx) {
        m_cancelRadiusPx = m_freezeRadiusPx;
    }
}

void MouseDwellMove::setCancelRadiusPx(int px)
{
    m_cancelRadiusPx = qMax(m_freezeRadiusPx, px);
}

void MouseDwellMove::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    // Map board-style flags onto reticle colors the user configured for mouse.
    if (m_reticle) {
        m_reticle->setVisuals(visuals);
    }
}

void MouseDwellMove::resetDwell()
{
    m_tracking = false;
    m_lastSampleMs = -1;
    m_progress = 0.0;
    emit progressChanged(0.0);
}

void MouseDwellMove::updateReticle(const QPointF& screen, double progress)
{
    if (!m_reticle) {
        return;
    }
    m_reticle->setProgress(progress);
    m_reticle->place(QPoint(qRound(screen.x()), qRound(screen.y())));
}

void MouseDwellMove::onGaze(const GazePoint& point, bool overBoard)
{
    if (!m_armed || !point.valid || overBoard) {
        if (m_reticle) {
            m_reticle->hide();
        }
        resetDwell();
        return;
    }

    const QPointF g(point.x, point.y);
    const qint64 now = m_clock.elapsed();
    const double dtSec =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.08);
    m_lastSampleMs = now;

    if (!m_tracking) {
        m_tracking = true;
        m_smoothPos = g;
        m_commitPos = g;
        m_progress = 0.0;
        updateReticle(m_smoothPos, 0.0);
        emit progressChanged(0.0);
        return;
    }

    // Smooth reticle follow — small gaze moves feel continuous, not jumpy.
    m_smoothPos.setX(m_smoothPos.x() * (1.0 - m_followAlpha) + g.x() * m_followAlpha);
    m_smoothPos.setY(m_smoothPos.y() * (1.0 - m_followAlpha) + g.y() * m_followAlpha);

    const double dx = m_smoothPos.x() - m_commitPos.x();
    const double dy = m_smoothPos.y() - m_commitPos.y();
    const double dist = qSqrt(dx * dx + dy * dy);

    if (dist <= double(m_stableRadiusPx)) {
        // On target: progress fills; commit center slowly tracks so tiny aims adjust.
        m_commitPos.setX(m_commitPos.x() * (1.0 - m_commitTrackAlpha)
                         + m_smoothPos.x() * m_commitTrackAlpha);
        m_commitPos.setY(m_commitPos.y() * (1.0 - m_commitTrackAlpha)
                         + m_smoothPos.y() * m_commitTrackAlpha);
        m_progress = qBound(0.0, m_progress + dtSec * 1000.0 / double(m_dwellMs), 1.0);
    } else if (dist <= double(m_freezeRadiusPx)) {
        // Medium drift: hold progress, keep reticle moving (do not snap/reset).
    } else if (dist <= double(m_cancelRadiusPx)) {
        // Larger drift: reverse progress smoothly.
        m_progress = qBound(0.0,
                           m_progress - dtSec * 1000.0 / double(m_dwellMs) * m_reverseScale,
                           1.0);
    } else {
        // Far off: reverse faster; when empty, re-home commit to current gaze.
        m_progress = qBound(0.0,
                           m_progress - dtSec * 1000.0 / double(m_dwellMs) * m_reverseScale * 1.6,
                           1.0);
        if (m_progress <= 0.001) {
            m_commitPos = m_smoothPos;
            m_progress = 0.0;
        }
    }

    updateReticle(m_smoothPos, m_progress);
    emit progressChanged(m_progress);

    if (m_progress < 1.0) {
        return;
    }

    // Fire at the smoothed reticle position (where the user is actually looking).
    const QPoint target(qRound(m_smoothPos.x()), qRound(m_smoothPos.y()));
    QString err;
    if (MouseInjector::moveTo(target.x(), target.y(), &err)) {
        GAZER_INFO << "MouseDwellMove →" << target.x() << target.y();
        emit movedTo(target);
    } else {
        GAZER_WARN << "MouseDwellMove failed:" << err;
    }

    // One-shot: disarm after successful move (re-arm from mouse board / LTS).
    setArmed(false);
}

} // namespace gazer
