#pragma once

#include "ui/OverlaySurface.h"

#include <QRect>
#include <QVector>

namespace gazer {

/// Full-desktop semi-transparent border while dwell is suspended, with a gap
/// at the unpause target (look-to-resume cue).
class DwellSuspendOverlay final : public OverlaySurface {
    Q_OBJECT

public:
    explicit DwellSuspendOverlay(QWidget* parent = nullptr);

    void setSuspended(bool suspended);
    [[nodiscard]] bool isSuspended() const { return m_suspended; }

    /// Screen-space rect(s) where the border is broken (unpause cells).
    void setGapRects(const QVector<QRect>& gaps);

    void refreshGeometry();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_suspended = false;
    QVector<QRect> m_gaps;
    int m_borderPx = 6;
};

} // namespace gazer
