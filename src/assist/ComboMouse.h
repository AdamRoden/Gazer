#pragma once

#include "assist/ComboMouseHit.h"
#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

/// Gaze pie HUD: dwell to place, then deadzone / inner drift annulus / outer command annulus.
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
    void setHoldFn(HoldFn fn) { m_hold = std::move(fn); }
    void setHeldQuery(HeldQuery q) { m_heldQuery = std::move(q); }
    void setScanGraceMs(int ms);
    void setDwellGraceMs(int ms);
    void setDwellMs(int ms);
    void setDwellSequence(const QVector<int>& ms);
    void setAccent(const QColor& c);
    void setTheme(const ThemeColors& theme) { m_theme = theme; }
    void setProgressVisuals(const ProgressVisuals& v) { m_progress = v; }
    void setRadii(int innerPx, int sharedPx, int outerPx);
    void setAnnulusColors(const QColor& inner, const QColor& outer);
    void setPaused(bool paused);

    /// Screen rect around the last origin (used to gate place-cursor after Move).
    [[nodiscard]] QRect originGateRect() const;
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

    [[nodiscard]] ComboMouseHit::Layout layout() const;
    [[nodiscard]] QRectF screenRect() const;
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
    void adoptLayout(const ComboMouseHit::Layout& L);
    void pushOverlay(const ComboMouseHit::Layout& L, ComboMouseHit::Band band,
                     ComboMouseHit::Slice slice, double dwellProg, QPointF dir);

    bool m_enabled = false;
    bool m_wheelVisible = false;
    bool m_localHeld = false;
    bool m_paused = false;
    QPoint m_origin;
    ComboMouseHit::Layout m_layout;
    ComboMouseHit::Band m_band = ComboMouseHit::Band::Deadzone;
    ComboMouseHit::Slice m_slice = ComboMouseHit::Slice::Right;
    double m_innerPx = ComboMouseHit::kHoleRadiusPx;
    double m_sharedPx = ComboMouseHit::kRingOuterPx;
    double m_outerPx = ComboMouseHit::kPieOuterPx;
    QColor m_innerColor = ComboMouseHit::kDefaultInnerFill;
    QColor m_outerColor = ComboMouseHit::kDefaultOuterFill;
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
