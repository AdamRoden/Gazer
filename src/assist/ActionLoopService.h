#pragma once

#include "layout/LayoutTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <functional>

namespace gazer {

/// Single sticky-mode policy for layout action loops and assist sticky modes.
///
/// Layout `actionLoop`: first activate starts a perpetual action series; second stops.
/// Engage latch ignores further activations while gaze stays on the item after a toggle.
///
/// Assist sticky modes (e.g. gaze click loop via MouseDwellMove): external code calls
/// setAssistSticky / clearAssistSticky with an activeState key. stopAll clears both.
/// isActiveState reports series loops and assist stickies under one registry.
class ActionLoopService final : public QObject {
    Q_OBJECT

public:
    using DispatchFn = std::function<void(const QVector<LayoutAction>& actions,
                                          const QString& sourceInstanceId)>;

    explicit ActionLoopService(QObject* parent = nullptr);

    void setDispatchFn(DispatchFn fn) { m_dispatch = std::move(fn); }

    /// Toggle loop for item. Returns true if now running.
    /// While gaze remains on the item after a toggle, further activations are ignored
    /// (dwell last-step would otherwise flip the loop every step).
    bool toggle(const QString& instanceId, const LayoutItem& item);

    /// Returns false if there are no actions (does not latch / does not start).
    bool start(const QString& instanceId, const LayoutItem& item);
    void stop(const QString& instanceId, const QString& itemId);
    void stopInstance(const QString& instanceId);
    /// Stops all series loops and clears assist sticky keys (not MouseDwellMove itself —
    /// caller should disarm click-loop when handling stopAllActionLoops).
    void stopAll();

    /// Clear post-toggle latch so the next dwell can toggle again (call on gaze leave).
    void clearEngageLatch(const QString& instanceId, const QString& itemId);
    void clearEngageLatchForInstance(const QString& instanceId);

    /// Register/clear an assist sticky mode under the shared activeState registry
    /// (e.g. "loop.gazeClick" for MouseDwellMove::CursorMoveClickLoop).
    void setAssistSticky(const QString& activeStateKey, bool on);
    void clearAssistSticky(const QString& activeStateKey);

    [[nodiscard]] bool isActive(const QString& instanceId, const QString& itemId) const;
    [[nodiscard]] bool isActiveState(const QString& activeStateKey) const;
    [[nodiscard]] bool anyActive() const
    {
        return !m_loops.isEmpty() || !m_assistSticky.isEmpty();
    }

signals:
    void loopsChanged();

private:
    struct LoopEntry {
        QString instanceId;
        QString itemId;
        QString activeStateKey;
        QVector<LayoutAction> actions;
        int stepIndex = 0;
        QTimer* timer = nullptr;
    };

    [[nodiscard]] static QString keyFor(const QString& instanceId, const QString& itemId);
    void scheduleNext(const QString& key);
    void runStep(const QString& key);
    void eraseKey(const QString& key);

    DispatchFn m_dispatch;
    QHash<QString, LoopEntry> m_loops;
    /// Keys that have toggled this engagement; cleared when gaze leaves the item.
    QSet<QString> m_engageLatch;
    /// Assist sticky activeState keys (not series loops).
    QSet<QString> m_assistSticky;
};

} // namespace gazer
