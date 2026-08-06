#pragma once

#include "core/GazePoint.h"
#include "layout/DwellRegionSpace.h"
#include "layout/DwellStateMachine.h"
#include "layout/LayoutTypes.h"
#include "ui/EdgeBubbleOverlay.h"
#include "ui/LayoutWindow.h"
#include "ui/ProgressVisuals.h"

#include <QObject>
#include <QPointF>
#include <QRect>
#include <QSet>
#include <QString>
#include <QVector>
#include <memory>

namespace gazer {

/// One live board: document + window + dwell.
class LayoutInstance final : public QObject {
    Q_OBJECT

public:
    LayoutInstance(QString instanceId, LayoutDocument document, QObject* parent = nullptr);

    [[nodiscard]] QString instanceId() const { return m_instanceId; }
    [[nodiscard]] QString layoutId() const { return m_document.id; }
    [[nodiscard]] const LayoutDocument& document() const { return m_document; }
    [[nodiscard]] LayoutWindow* window() const { return m_window.get(); }

    void setDocument(LayoutDocument document);
    void raise();
    void hide();
    void applyPlacement(int cascadeOffset = 0);
    void placeRelative(int offsetX, int offsetY);

    void setGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setActiveItemIds(const QSet<QString>& activeIds);
    void setItemText(const QString& itemId, const QString& label, const QString& caption = {});

    [[nodiscard]] QString hitTest(const QPointF& screenPoint) const;
    [[nodiscard]] bool containsScreenPoint(const QPointF& screenPoint) const;
    [[nodiscard]] QRect screenRect() const;
    [[nodiscard]] QPointF centerScreen() const;

    void feedGaze(const GazePoint& point, const QString& itemIdUnderGaze);
    void leaveGaze();

    void setEdgeBubbleOverlay(EdgeBubbleOverlay* overlay) { m_edgeBubbles = overlay; }

signals:
    void itemActivated(const QString& instanceId, const QString& itemId);
    void windowCloseRequested(const QString& instanceId);

private:
    void applyDwellConfig();
    void applyDwellForItem(const QString& itemId);
    void syncEdgeBubble(const QString& itemId, double progress);
    void clearEdgeBubble();

    [[nodiscard]] QPoint boardOrigin() const;
    [[nodiscard]] QScreen* boardScreen() const;
    [[nodiscard]] DwellRegionSpace::Resolved resolveItem(const LayoutItem& item) const;
    [[nodiscard]] QRect gridItemScreenRect(const QString& itemId) const;

    QString m_instanceId;
    LayoutDocument m_document;
    std::unique_ptr<LayoutWindow> m_window;
    std::unique_ptr<DwellStateMachine> m_dwell;
    QVector<int> m_globalDwellSequence;
    int m_globalGraceMs = 0;
    QString m_activeDwellItemId;
    EdgeBubbleOverlay* m_edgeBubbles = nullptr;
};

} // namespace gazer
