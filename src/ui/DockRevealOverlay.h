#pragma once

#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "ui/OverlaySurface.h"
#include "ui/Theme.h"

#include <QColor>
#include <QRect>

namespace gazer {

/// Bottom-left dwell strip used while Main is collapsed and hidden.
/// Completing dwell reveals the dock chip above this strip.
class DockRevealOverlay final : public OverlaySurface {
    Q_OBJECT

public:
    explicit DockRevealOverlay(QObject* parent = nullptr);

    void setEnabledReveal(bool enabled);
    [[nodiscard]] bool isEnabledReveal() const { return m_enabled; }

    void setDwellMs(int ms);
    void setAccent(const QColor& c);

    /// Screen-space hit test (does not require looking only at taskbar pixels).
    [[nodiscard]] bool containsGaze(const GazePoint& point) const;
    void onGaze(const GazePoint& point);

    /// Logical global rect of the hot zone (for debugging / placement).
    [[nodiscard]] QRect hotScreenRect() const { return m_hotScreenRect; }

signals:
    void dockRevealRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void syncPlacement();
    void ensureVisible();

    bool m_enabled = false;
    int m_hotW = 360;
    int m_hotH = 140;
    QRect m_hotScreenRect;
    DwellStateMachine m_dwell;
    QString m_hoverId;
    double m_progress = 0.0;
    QColor m_accent = ThemeColors::defaultProgressColor();
};

} // namespace gazer
