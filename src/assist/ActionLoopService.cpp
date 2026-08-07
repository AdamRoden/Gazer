#include "assist/ActionLoopService.h"

#include "utils/Log.h"

#include <algorithm>

namespace gazer {

ActionLoopService::ActionLoopService(QObject* parent)
    : QObject(parent)
{
}

QString ActionLoopService::keyFor(const QString& instanceId, const QString& itemId)
{
    return instanceId + QLatin1Char('\x1f') + itemId;
}

bool ActionLoopService::toggle(const QString& instanceId, const LayoutItem& item)
{
    const QString k = keyFor(instanceId, item.id);
    // Ignore dwell hold-repeat while still engaged after the last toggle.
    if (m_engageLatch.contains(k)) {
        return m_loops.contains(k);
    }

    if (m_loops.contains(k)) {
        m_engageLatch.insert(k);
        stop(instanceId, item.id);
        return false;
    }

    if (!start(instanceId, item)) {
        // Do not latch a failed start — user can re-try immediately.
        return false;
    }
    m_engageLatch.insert(k);
    return true;
}

void ActionLoopService::clearEngageLatch(const QString& instanceId, const QString& itemId)
{
    m_engageLatch.remove(keyFor(instanceId, itemId));
}

void ActionLoopService::clearEngageLatchForInstance(const QString& instanceId)
{
    QStringList keys;
    for (const QString& k : m_engageLatch) {
        if (k.startsWith(instanceId + QLatin1Char('\x1f'))) {
            keys.push_back(k);
        }
    }
    for (const QString& k : keys) {
        m_engageLatch.remove(k);
    }
}

bool ActionLoopService::start(const QString& instanceId, const LayoutItem& item)
{
    const QVector<LayoutAction> acts = item.effectiveActions();
    if (acts.isEmpty()) {
        GAZER_WARN << "ActionLoop: no actions for" << item.id;
        return false;
    }

    const QString k = keyFor(instanceId, item.id);
    if (m_loops.contains(k)) {
        stop(instanceId, item.id);
    }

    LoopEntry e;
    e.instanceId = instanceId;
    e.itemId = item.id;
    e.activeStateKey = item.loopActiveStateKey();
    e.actions = acts;
    e.stepIndex = 0;
    e.timer = new QTimer(this);
    e.timer->setSingleShot(true);
    connect(e.timer, &QTimer::timeout, this, [this, k]() { runStep(k); });

    m_loops.insert(k, e);
    GAZER_INFO << "ActionLoop START" << instanceId << item.id << "steps" << acts.size();
    emit loopsChanged();
    scheduleNext(k);
    return true;
}

void ActionLoopService::stop(const QString& instanceId, const QString& itemId)
{
    eraseKey(keyFor(instanceId, itemId));
}

void ActionLoopService::stopInstance(const QString& instanceId)
{
    QStringList keys;
    for (auto it = m_loops.constBegin(); it != m_loops.constEnd(); ++it) {
        if (it.value().instanceId == instanceId) {
            keys.push_back(it.key());
        }
    }
    for (const QString& k : keys) {
        eraseKey(k);
    }
    clearEngageLatchForInstance(instanceId);
}

void ActionLoopService::stopAll()
{
    const QStringList keys = m_loops.keys();
    for (const QString& k : keys) {
        eraseKey(k);
    }
    m_engageLatch.clear();
    if (!m_assistSticky.isEmpty()) {
        m_assistSticky.clear();
        emit loopsChanged();
    }
}

void ActionLoopService::setAssistSticky(const QString& activeStateKey, bool on)
{
    if (activeStateKey.isEmpty()) {
        return;
    }
    const bool had = m_assistSticky.contains(activeStateKey);
    if (on && !had) {
        m_assistSticky.insert(activeStateKey);
        emit loopsChanged();
    } else if (!on && had) {
        m_assistSticky.remove(activeStateKey);
        emit loopsChanged();
    }
}

void ActionLoopService::clearAssistSticky(const QString& activeStateKey)
{
    setAssistSticky(activeStateKey, false);
}

bool ActionLoopService::isActive(const QString& instanceId, const QString& itemId) const
{
    return m_loops.contains(keyFor(instanceId, itemId));
}

bool ActionLoopService::isActiveState(const QString& activeStateKey) const
{
    if (activeStateKey.isEmpty()) {
        return false;
    }
    if (m_assistSticky.contains(activeStateKey)) {
        return true;
    }
    for (const LoopEntry& e : m_loops) {
        if (e.activeStateKey == activeStateKey) {
            return true;
        }
    }
    return false;
}

void ActionLoopService::eraseKey(const QString& key)
{
    auto it = m_loops.find(key);
    if (it == m_loops.end()) {
        return;
    }
    if (it->timer) {
        it->timer->stop();
        it->timer->deleteLater();
        it->timer = nullptr;
    }
    GAZER_INFO << "ActionLoop STOP" << it->instanceId << it->itemId;
    m_loops.erase(it);
    emit loopsChanged();
}

void ActionLoopService::scheduleNext(const QString& key)
{
    auto it = m_loops.find(key);
    if (it == m_loops.end() || !it->timer) {
        return;
    }
    if (it->stepIndex < 0 || it->stepIndex >= it->actions.size()) {
        it->stepIndex = 0;
    }
    int delay = std::max(0, it->actions[it->stepIndex].delayMs);
    if (delay <= 0) {
        delay = (it->stepIndex == 0) ? 120 : 30;
    }
    it->timer->start(delay);
}

void ActionLoopService::runStep(const QString& key)
{
    auto it = m_loops.find(key);
    if (it == m_loops.end()) {
        return;
    }
    if (!m_dispatch || it->actions.isEmpty()) {
        eraseKey(key);
        return;
    }

    // Snapshot before dispatch — callbacks may erase this loop (closeLayout, stopAll, …).
    if (it->stepIndex < 0 || it->stepIndex >= it->actions.size()) {
        it->stepIndex = 0;
    }
    const int step = it->stepIndex;
    const QString instanceId = it->instanceId;
    LayoutAction fire = it->actions[step];
    fire.delayMs = 0;

    m_dispatch(QVector<LayoutAction>{fire}, instanceId);

    // Re-find after reentrant dispatch; abort if the loop was stopped.
    it = m_loops.find(key);
    if (it == m_loops.end()) {
        return;
    }
    it->stepIndex = (step + 1) % it->actions.size();
    scheduleNext(key);
}

} // namespace gazer
