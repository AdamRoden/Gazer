#pragma once

#include "core/GazePoint.h"
#include "layout/DwellRegionSpace.h"
#include "layout/DwellStateMachine.h"
#include "layout/LayoutTypes.h"
#include "ui/EdgeBubbleOverlay.h"
#include "ui/LayoutQuickWindow.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <functional>
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
    [[nodiscard]] LayoutQuickWindow* window() const { return m_window.get(); }

    void setPropertyContext(const QVariantMap& props);

    void setDocument(LayoutDocument document);
    void raise();
    void hide();
    void forceHide();

    /// Bottom-anchored scale, min → 1 (drawerMotion boards). No opacity animation.
    void playAppear();
    void playDismiss(std::function<void()> onDone = {});
    [[nodiscard]] bool usesDrawerMotion() const;
    [[nodiscard]] bool isScaleAnimating() const;
    [[nodiscard]] bool isDismissing() const;
    void applyPlacement();
    void placeRelative(int offsetX, int offsetY);

    void setGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs,
                                int scanGraceMs = DwellStateMachine::kDefaultScanGraceMs);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setTheme(const ThemeColors& theme);
    void setActiveItemIds(const QSet<QString>& activeIds);
    void setItemText(const QString& itemId, const QString& label, const QString& caption = {});
    void mutateItems(const std::function<void(LayoutItem&)>& fn);

    [[nodiscard]] QString hitTest(const QPointF& screenPoint) const;
    [[nodiscard]] bool containsScreenPoint(const QPointF& screenPoint) const;
    /// True when this instance is showing unbounded progress chrome under @p screenPoint.
    /// Used so progress hit beats other boards' windows regardless of z-order.
    [[nodiscard]] bool containsVisibleUnboundedProgress(const QPointF& screenPoint) const;
    [[nodiscard]] QRect screenRect() const;
    /// Screen-space hit rect for a board item (grid cell or dwell region).
    [[nodiscard]] QRect itemScreenRect(const QString& itemId) const;
    [[nodiscard]] QPointF centerScreen() const;
    /// On-screen affordance for unpause gap (edge band or hit if visible).
    [[nodiscard]] QRect unpauseGapScreenRect(const LayoutItem& item) const;

    void feedGaze(const GazePoint& point, const QString& itemIdUnderGaze);
    void leaveGaze();

    void setEdgeBubbleOverlay(EdgeBubbleOverlay* overlay) { m_edgeBubbles = overlay; }

    /// When true, only isDwellExempt items are dwell/click hit-tested.
    void setDwellSuspended(bool suspended) { m_dwellSuspended = suspended; }
    [[nodiscard]] bool isDwellSuspended() const { return m_dwellSuspended; }

    // --- Auto-close (secondaries) ---
    /// Idle → instant 50% hold for fadeMs → 500ms dismiss shrink → close.
    void setAutoCloseEnabled(bool on) { m_autoCloseEnabled = on; }
    [[nodiscard]] bool autoCloseEnabled() const { return m_autoCloseEnabled; }
    void setAutoCloseTiming(int idleMs, int fadeMs);
    void resetAutoCloseClock(qint64 nowMs);
    /// 1.0 until idle elapses, then 0.5 for the hold and through the shrink.
    [[nodiscard]] double autoCloseOpacity(qint64 nowMs) const;
    /// True when the dismiss shrink has finished (secondaries).
    [[nodiscard]] bool autoCloseFinished(qint64 nowMs) const;
    [[nodiscard]] bool autoCloseIdleElapsed(qint64 nowMs) const;
    /// True after idle + hold; drawer collapse / secondary shrink may start.
    [[nodiscard]] bool autoCloseReadyToDismiss(qint64 nowMs) const;
    /// 50% hold, then start dismiss shrink (non-drawer boards).
    void applyAutoCloseVisuals(qint64 nowMs);
    void setFadeOpacity(double opacity);

signals:
    void itemActivated(const QString& instanceId, const QString& itemId);
    void windowCloseRequested(const QString& instanceId);
    /// Gaze left an item (or board); used so sticky toggles/loops can arm again.
    void dwellEngagementEnded(const QString& instanceId, const QString& itemId);
    /// Any dwell progress on this instance (resets auto-close idle).
    void dwellActivity(const QString& instanceId);

private:
    void applyDwellConfig();
    void applyDwellForItem(const QString& itemId);
    /// Hit-policy: engage off-screen drift lip after continuous dwell on logical rect.
    void updateDriftLip(const QString& itemId, double progress);
    void syncEdgeBubble(const QString& itemId, double progress);
    void clearEdgeBubble();

    [[nodiscard]] QPoint boardOrigin() const;
    [[nodiscard]] QScreen* boardScreen() const;
    [[nodiscard]] QRect boundsRectFor(BoundsMode mode) const;
    [[nodiscard]] DwellRegionSpace::Resolved resolveItem(const LayoutItem& item) const;
    [[nodiscard]] QRect itemHitRect(const LayoutItem& item) const;
    [[nodiscard]] QRect gridItemScreenRect(const QString& itemId) const;
    /// Visible progress strip / band for unbounded items (hit-tested while dwelling).
    [[nodiscard]] QRect progressHitRect(const LayoutItem& item, double progress) const;
    [[nodiscard]] ProgressVisuals resolvedProgressVisuals(const LayoutItem* item) const;

    [[nodiscard]] bool itemShown(const LayoutItem& item) const;
    void cancelScaleAnim(bool invokeDone);
    void tickScaleAnim();
    /// Scale about the placement pin: drawer / bottom anchors keep the bottom edge.
    void applyScale(double scale);

    QString m_instanceId;
    LayoutDocument m_document;
    QVariantMap m_props;
    std::unique_ptr<LayoutQuickWindow> m_window;
    std::unique_ptr<DwellStateMachine> m_dwell;
    ProgressVisuals m_progressVisuals;
    QVector<int> m_globalDwellSequence;
    int m_globalGraceMs = 0;
    int m_globalScanGraceMs = DwellStateMachine::kDefaultScanGraceMs;
    QString m_activeDwellItemId;
    /// Off-screen item id for which the on-screen drift lip is active (after engage delay).
    QString m_dwellLipItemId;
    /// Item currently accumulating time toward lip engage (may not yet have lip).
    QString m_dwellLipTrackItemId;
    QElapsedTimer m_dwellLipClock;
    bool m_dwellSuspended = false;
    EdgeBubbleOverlay* m_edgeBubbles = nullptr;
    double m_lastProgress = 0.0;

    bool m_autoCloseEnabled = false;
    int m_autoCloseIdleMs = 10000;
    int m_autoCloseFadeMs = 3000;
    qint64 m_lastActivityMs = 0;
    bool m_autoCloseDismissActive = false;
    bool m_autoCloseDismissDone = false;

    /// Dwell on logical rect this long before extending hit into the edge lip.
    static constexpr int kOffscreenLipEngageMs = 100;
    enum class ScalePhase { Idle, Appear, Dismiss };
    ScalePhase m_scalePhase = ScalePhase::Idle;
    QTimer m_scaleTimer;
    QElapsedTimer m_scaleClock;
    QRect m_scaleTargetGeom;
    std::function<void()> m_scaleDone;

    static constexpr double kDrawerMinScale = 0.05;
    static constexpr int kDrawerAppearMs = 420;
    static constexpr int kDrawerDismissMs = 500;
    static constexpr double kAutoCloseFadeFloor = 0.5;
};

} // namespace gazer
