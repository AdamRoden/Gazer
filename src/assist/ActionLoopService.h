#pragma once

#include "layout/PageTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <functional>

namespace gazer {

/// Sticky loops for page targets, plus assist sticky keys (gaze click loop).
class ActionLoopService final : public QObject {
    Q_OBJECT

public:
    using DispatchFn = std::function<void(const QVector<PageAction>& actions, const QString& pageId)>;

    explicit ActionLoopService(QObject* parent = nullptr);

    void setDispatchFn(DispatchFn fn) { m_dispatch = std::move(fn); }

    bool toggle(const QString& pageId, const QString& targetId, const QVector<PageAction>& actions,
                const QString& activeStateKey);
    bool start(const QString& pageId, const QString& targetId, const QVector<PageAction>& actions,
               const QString& activeStateKey);
    void stop(const QString& pageId, const QString& targetId);
    void stopPage(const QString& pageId);
    void stopAll();

    void clearEngageLatch(const QString& pageId, const QString& targetId);
    void clearEngageLatchForPage(const QString& pageId);
    void clearAllEngageLatches() { m_engageLatch.clear(); }

    void setAssistSticky(const QString& activeStateKey, bool on);
    void clearAssistSticky(const QString& activeStateKey);

    [[nodiscard]] bool isActive(const QString& pageId, const QString& targetId) const;
    [[nodiscard]] bool isActiveState(const QString& activeStateKey) const;
    [[nodiscard]] bool anyActive() const
    {
        return !m_loops.isEmpty() || !m_assistSticky.isEmpty();
    }

signals:
    void loopsChanged();

private:
    struct LoopEntry {
        QString pageId;
        QString targetId;
        QString activeStateKey;
        QVector<PageAction> actions;
        int stepIndex = 0;
        QTimer* timer = nullptr;
    };

    [[nodiscard]] static QString keyFor(const QString& pageId, const QString& targetId);
    void scheduleNext(const QString& key);
    void runStep(const QString& key);
    void eraseKey(const QString& key);

    DispatchFn m_dispatch;
    QHash<QString, LoopEntry> m_loops;
    QSet<QString> m_engageLatch;
    QSet<QString> m_assistSticky;
};

} // namespace gazer
