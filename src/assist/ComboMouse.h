#pragma once

#include "assist/ComboMouseHit.h"
#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "ui/OverlaySurface.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

/// Gaze pie HUD: dwell to place, then deadzone / yellow drift ring / command slices.
class ComboMouse final : public QObject {
    Q_OBJECT

public:
    using HoldFn = std::function<bool(bool down)>;
    using HeldQuery = std::function<bool()>;

    explicit ComboMouse(QObject* parent = nullptr);
    ~ComboMouse() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] bool isWheelVisible() const { return m_wheelVisible; }
    [[nodiscard]] bool isDragHeld() const;
    [[nodiscard]] OverlaySurface* overlay() const;

    void setHoldFn(HoldFn fn) { m_hold = std::move(fn); }
    void setHeldQuery(HeldQuery q) { m_heldQuery = std::move(q); }
    void setScanGraceMs(int ms);
    void setDwellGraceMs(int ms);
    void setDwellMs(int ms);
    void setDwellSequence(const QVector<int>& ms);
    void setAccent(const QColor& c);
    void setTheme(const ThemeColors& theme) { m_theme = theme; }
    void setProgressVisuals(const ProgressVisuals& v) { m_progress = v; }
    void setPaused(bool paused);

    /// Show the wheel at @p pos (after a place dwell). Pins the OS cursor to the hole.
    void showAt(const QPoint& pos);
    /// Hide the wheel and ask the host to re-arm place-cursor (Move slice).
    void requestMove();

    [[nodiscard]] bool containsGaze(const GazePoint& point) const;
    /// @p pauseInput: over Gazer chrome that is not this overlay.
    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(bool enabled);
    void wheelVisibleChanged(bool visible);
    void placeRequested();

private:
    class WheelOverlay;

    struct Metrics {
        double deadzone = 60.0;
        double ringOuter = 120.0;
        double pieOuter = 220.0;
    };

    [[nodiscard]] Metrics metrics() const;
    [[nodiscard]] double screenHeightPx() const;
    void hideWheel();
    void pinCursor();
    void moveOrigin(const QPoint& pos);
    void clampOrigin();
    void fireSlice(ComboMouseHit::Slice slice);
    void nudgeToward(const QPointF& dir);
    void onActivated(const QString& id);
    [[nodiscard]] static QString hitId(const ComboMouseHit::Result& h);
    void setDragHeld(bool held);
    void releaseDrag();
    [[nodiscard]] bool holdingLeft() const;

    bool m_enabled = false;
    bool m_wheelVisible = false;
    bool m_localHeld = false;
    bool m_paused = false;
    QPoint m_origin;
    QColor m_accent;
    ThemeColors m_theme = ThemeColors::darkPreset();
    ProgressVisuals m_progress;
    HoldFn m_hold;
    HeldQuery m_heldQuery;
    DwellStateMachine m_dwell;
    QPointF m_nudgeDir;
    QElapsedTimer m_clock;
    std::unique_ptr<WheelOverlay> m_overlay;
};

} // namespace gazer
