#pragma once

#include "layout/DwellRegionSpace.h"
#include "ui/OverlaySurface.h"

#include <QColor>
#include <QHash>
#include <QString>

namespace gazer {

/// Full-screen overlay: one half-ellipse bubble per layout instance key.
/// Geometry comes from DwellRegionSpace::EdgeBand (shared with hit-test).
class EdgeBubbleOverlay final : public OverlaySurface {
    Q_OBJECT

public:
    struct Bubble {
        QString key; // typically instanceId
        QString label;
        DwellRegionSpace::EdgeBand band;
        double progress = 0.0;
        QColor color = QColor(0, 220, 255);
    };

    explicit EdgeBubbleOverlay(QObject* parent = nullptr);

    /// Upsert bubble for key (one slot per key).
    void setBubble(const Bubble& bubble);
    void clearBubble(const QString& key);
    void clearAll();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void syncGeometry();
    void connectScreenSignals();

    QHash<QString, Bubble> m_bubbles;
};

} // namespace gazer
