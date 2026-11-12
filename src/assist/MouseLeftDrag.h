#pragma once

#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "ui/ProgressVisuals.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtMath>

#include <memory>

namespace gazer {

class MouseAssistState;

/// Choice shown after the drag end is fixated.
enum class DragChoice { Move, Release, Cancel };

struct DragFanButton {
    DragChoice choice = DragChoice::Release;
    QPointF center;
};

/// Three round buttons on a semicircle facing screen center (or straight up
/// when the anchor is already near the center).
struct DragFan {
    QPointF anchor;
    QPointF facing = QPointF(0, -1);
    DragFanButton buttons[3];
    double buttonRadius = 52;
    double hitRadius = 64;
};

inline constexpr double kDragButtonRadius = 52.0;
inline constexpr double kDragOrbitRadius = 150.0;
inline constexpr double kDragFanSpreadDeg = 70.0;
inline constexpr double kDragHitExtra = 12.0;
inline constexpr double kDragScreenMargin = 8.0;
/// Endpoint fixation relative to move-to-gaze dwell. Choice buttons stay at that dwell.
inline constexpr int kDragFinishDwellScale = 2;

[[nodiscard]] inline QPointF dragFanUnit(QPointF v, QPointF fallback)
{
    const double len = std::hypot(v.x(), v.y());
    if (len < 1e-3) {
        return fallback;
    }
    return v / len;
}

[[nodiscard]] inline bool dragFanFits(const DragFan& fan, const QRectF& screen)
{
    const double m = kDragScreenMargin;
    for (const DragFanButton& b : fan.buttons) {
        const double r = fan.buttonRadius;
        if (b.center.x() - r < screen.left() + m || b.center.y() - r < screen.top() + m
            || b.center.x() + r > screen.right() - m || b.center.y() + r > screen.bottom() - m) {
            return false;
        }
    }
    return true;
}

/// Semicircle of Release / Move / Cancel. Move sits on the axis toward the
/// screen center. The orbit shrinks, then centers clamp, so the circles stay
/// on screen and off the anchor.
[[nodiscard]] inline DragFan layoutDragFan(QPointF anchor, const QRectF& screen,
                                           double buttonRadius = kDragButtonRadius,
                                           double orbitRadius = kDragOrbitRadius)
{
    QPointF dir = QPointF(screen.center()) - anchor;
    if (std::hypot(dir.x(), dir.y()) < 48.0) {
        dir = QPointF(0, -1);
    } else {
        dir = dragFanUnit(dir, QPointF(0, -1));
    }
    const QPointF left(dir.y(), -dir.x());
    const DragChoice choices[3] = {DragChoice::Release, DragChoice::Move, DragChoice::Cancel};
    const double angles[3] = {kDragFanSpreadDeg, 0.0, -kDragFanSpreadDeg};
    const double minAnchor = buttonRadius + 36.0;

    auto place = [&](double orbit) {
        DragFan fan;
        fan.anchor = anchor;
        fan.facing = dir;
        fan.buttonRadius = buttonRadius;
        fan.hitRadius = buttonRadius + kDragHitExtra;
        for (int i = 0; i < 3; ++i) {
            const double a = qDegreesToRadians(angles[i]);
            fan.buttons[i].choice = choices[i];
            fan.buttons[i].center =
                anchor + (std::cos(a) * dir + std::sin(a) * left) * orbit;
        }
        return fan;
    };

    double orbit = orbitRadius;
    DragFan fan = place(orbit);
    for (int attempt = 0; attempt < 8 && !dragFanFits(fan, screen); ++attempt) {
        const double next = orbit * 0.82;
        if (next < minAnchor) {
            break;
        }
        orbit = next;
        fan = place(orbit);
    }
    if (dragFanFits(fan, screen)) {
        return fan;
    }

    const QRectF inset = screen.adjusted(kDragScreenMargin + buttonRadius,
                                         kDragScreenMargin + buttonRadius,
                                         -(kDragScreenMargin + buttonRadius),
                                         -(kDragScreenMargin + buttonRadius));
    auto clampCenter = [&](QPointF c) {
        if (!inset.isValid() || inset.width() < 1 || inset.height() < 1) {
            return c;
        }
        c.setX(qBound(inset.left(), c.x(), inset.right()));
        c.setY(qBound(inset.top(), c.y(), inset.bottom()));
        return c;
    };
    for (DragFanButton& b : fan.buttons) {
        b.center = clampCenter(b.center);
        const QPointF away = b.center - anchor;
        const double d = std::hypot(away.x(), away.y());
        if (d < minAnchor) {
            const QPointF u = d < 1e-3 ? dir : away / d;
            b.center = clampCenter(anchor + u * minAnchor);
        }
    }
    const double minGap = buttonRadius * 2.0 + 16.0;
    for (int pass = 0; pass < 8; ++pass) {
        for (int i = 0; i < 3; ++i) {
            for (int j = i + 1; j < 3; ++j) {
                QPointF delta = fan.buttons[j].center - fan.buttons[i].center;
                double dist = std::hypot(delta.x(), delta.y());
                if (dist >= minGap) {
                    continue;
                }
                if (dist < 1e-3) {
                    delta = left;
                    dist = 1.0;
                }
                const QPointF u = delta / dist;
                const double push = (minGap - dist) * 0.5;
                fan.buttons[i].center = clampCenter(fan.buttons[i].center - u * push);
                fan.buttons[j].center = clampCenter(fan.buttons[j].center + u * push);
            }
        }
    }
    return fan;
}

/// Index of the button under @p gaze, or -1. The nearer center wins an overlap.
[[nodiscard]] inline int hitDragFan(const DragFan& fan, QPointF gaze)
{
    int best = -1;
    double bestD = 0.0;
    for (int i = 0; i < 3; ++i) {
        const double d = std::hypot(gaze.x() - fan.buttons[i].center.x(),
                                    gaze.y() - fan.buttons[i].center.y());
        if (d <= fan.hitRadius && (best < 0 || d < bestD)) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

/// Dwell-move, left button down, then a dotted arrow until the end is fixated.
/// That fixation is twice the move-to-gaze dwell. Choice buttons use the normal
/// dwell. Check releases at the end. X returns to the press point, waits 50 ms, and releases.
/// Mouse-move aims again.
class MouseLeftDrag final : public QObject {
    Q_OBJECT

public:
    explicit MouseLeftDrag(QObject* parent = nullptr);
    ~MouseLeftDrag() override;

    void setMouseAssist(MouseAssistState* assist) { m_assist = assist; }
    void setDwellMs(int ms);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setPaused(bool paused);

    [[nodiscard]] bool isActive() const { return m_phase != Phase::Idle; }
    [[nodiscard]] bool isAiming() const { return m_phase == Phase::Aim; }
    [[nodiscard]] bool isChoosing() const { return m_phase == Phase::Choose; }
    [[nodiscard]] bool containsChoice(const GazePoint& point) const;

    /// Left button down at @p press. The cursor is already there.
    [[nodiscard]] bool begin(const QPoint& press, QString* error = nullptr);
    void onGaze(const GazePoint& point);
    /// Move back to the press point and release. No-op when idle.
    void cancel();

signals:
    void activeChanged(bool active);

private:
    enum class Phase { Idle, Aim, Choose };
    class Overlay;

    void finish(bool moveBack);
    void enterChoose(const QPoint& end);
    void syncOverlay();
    void fitOverlay();

    MouseAssistState* m_assist = nullptr;
    Phase m_phase = Phase::Idle;
    bool m_paused = false;
    bool m_gateButtons = false;
    int m_dwellMs = 800;
    QPoint m_start;
    QPoint m_end;
    QPointF m_tip;
    DragFan m_fan;
    int m_hot = -1;
    double m_hotProg = 0.0;
    GazeDwellTracker m_dwell;
    QElapsedTimer m_clock;
    qint64 m_lastSampleMs = -1;
    InvalidGazeGrace m_invalidGrace;
    ProgressVisuals m_visuals;
    std::unique_ptr<Overlay> m_overlay;
};

} // namespace gazer
