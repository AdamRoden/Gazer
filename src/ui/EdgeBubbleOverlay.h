#pragma once

#include "layout/DwellRegionSpace.h"
#include "ui/OverlaySurface.h"
#include "ui/ProgressVisuals.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QTimer>

namespace gazer {

/// Full-screen overlay: progress chrome for unbounded / off-screen dwell regions.
/// Band position follows coerced dwell-region x/y; fill grows from center outward.
class EdgeBubbleOverlay final : public OverlaySurface {
    Q_OBJECT

public:
    struct Bubble {
        QString key; // typically instanceId
        QString label;
        DwellRegionSpace::EdgeBand band;
        double progress = 0.0;
        ProgressVisuals visuals;
        bool flashing = false;
    };

    explicit EdgeBubbleOverlay(QObject* parent = nullptr);

    void setBubble(const Bubble& bubble);
    void clearBubble(const QString& key);
    void clearAll();

    /// Flash fill+border then clear (activation complete).
    void flashThenClear(const QString& key, const ProgressVisuals& visuals, int flashMs = 140);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void syncGeometry();
    void connectScreenSignals();

    QHash<QString, Bubble> m_bubbles;
    QHash<QString, QTimer*> m_flashTimers;
};

} // namespace gazer
