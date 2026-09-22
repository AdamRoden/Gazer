#include "assist/ActionLoopService.h"

#include "utils/Log.h"

#include <algorithm>

namespace gazer {

ActionLoopService::ActionLoopService(QObject* parent)
    : QObject(parent)
{
}

QString ActionLoopService::keyFor(const QString& pageId, const QString& targetId)
{
    return pageId + QLatin1Char('\x1f') + targetId;
}

bool ActionLoopService::toggle(const QString& pageId, const QString& targetId,
                               const QVector<PageAction>& actions, const QString& activeStateKey)
{
    const QString k = keyFor(pageId, targetId);
    if (m_engageLatch.contains(k)) {
        return m_loops.contains(k);
    }
    if (m_loops.contains(k)) {
        m_engageLatch.insert(k);
        stop(pageId, targetId);
        return false;
    }
    if (!start(pageId, targetId, actions, activeStateKey)) {
        return false;
    }
    m_engageLatch.insert(k);
    return true;
}

void ActionLoopService::clearEngageLatch(const QString& pageId, const QString& targetId)
{
    m_engageLatch.remove(keyFor(pageId, targetId));
}

void ActionLoopService::clearEngageLatchForPage(const QString& pageId)
{
    QStringList keys;
    for (const QString& k : m_engageLatch) {
        if (k.startsWith(pageId + QLatin1Char('\x1f'))) {
            keys.push_back(k);
        }
    }
    for (const QString& k : keys) {
        m_engageLatch.remove(k);
    }
}

bool ActionLoopService::start(const QString& pageId, const QString& targetId,
                              const QVector<PageAction>& actions, const QString& activeStateKey)
{
    if (actions.isEmpty()) {
        GAZER_WARN << "ActionLoop: no actions for" << targetId;
        return false;
    }
    const QString k = keyFor(pageId, targetId);
    if (m_loops.contains(k)) {
        stop(pageId, targetId);
    }
    LoopEntry e;
    e.pageId = pageId;
    e.targetId = targetId;
    e.activeStateKey = activeStateKey.isEmpty() ? targetId : activeStateKey;
    e.actions = actions;
    e.stepIndex = 0;
    e.timer = new QTimer(this);
    e.timer->setSingleShot(true);
    e.started.start();
    connect(e.timer, &QTimer::timeout, this, [this, k]() { runStep(k); });
    m_loops.insert(k, e);
    GAZER_INFO << "ActionLoop START" << pageId << targetId << "steps" << actions.size();
    emit loopsChanged();
    scheduleNext(k);
    return true;
}

void ActionLoopService::stop(const QString& pageId, const QString& targetId)
{
    eraseKey(keyFor(pageId, targetId));
}

void ActionLoopService::stopPage(const QString& pageId)
{
    QStringList keys;
    for (auto it = m_loops.constBegin(); it != m_loops.constEnd(); ++it) {
        if (it.value().pageId == pageId) {
            keys.push_back(it.key());
        }
    }
    for (const QString& k : keys) {
        eraseKey(k);
    }
    clearEngageLatchForPage(pageId);
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

bool ActionLoopService::isActive(const QString& pageId, const QString& targetId) const
{
    return m_loops.contains(keyFor(pageId, targetId));
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
    GAZER_INFO << "ActionLoop STOP" << it->pageId << it->targetId;
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
    int delay = (it->stepIndex == 0) ? 120 : 30;
    it->timer->start(delay);
}

void ActionLoopService::runStep(const QString& key)
{
    auto it = m_loops.find(key);
    if (it == m_loops.end()) {
        return;
    }
    constexpr qint64 kMaxLoopMs = 60000;
    if (it->started.isValid() && it->started.elapsed() >= kMaxLoopMs) {
        GAZER_WARN << "ActionLoop timeout" << it->pageId << it->targetId;
        eraseKey(key);
        return;
    }
    if (!m_dispatch || it->actions.isEmpty()) {
        eraseKey(key);
        return;
    }
    if (it->stepIndex < 0 || it->stepIndex >= it->actions.size()) {
        it->stepIndex = 0;
    }
    const int step = it->stepIndex;
    const QString pageId = it->pageId;
    const PageAction fire = it->actions.at(step);
    m_dispatch(QVector<PageAction>{fire}, pageId);
    it = m_loops.find(key);
    if (it == m_loops.end()) {
        return;
    }
    it->stepIndex = (step + 1) % it->actions.size();
    scheduleNext(key);
}

} // namespace gazer
