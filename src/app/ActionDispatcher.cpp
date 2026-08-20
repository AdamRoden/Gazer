#include "app/ActionDispatcher.h"

#include "input/InputTypes.h"
#include "layout/SessionNavigate.h"
#include "utils/Log.h"

#include <QTimer>
#include <functional>
#include <memory>

namespace gazer {

ActionDispatcher::ActionDispatcher(GazerServices& services, QObject* parent)
    : QObject(parent)
    , m_svc(services)
{
}

void ActionDispatcher::dispatchItem(const LayoutItem& item, const QString& sourceInstanceId)
{
    if (item.actionLoop) {
        const bool wasActive = m_svc.actionLoops().isActive(sourceInstanceId, item.id);
        const bool on = m_svc.actionLoops().toggle(sourceInstanceId, item);
        const QString name = item.label.isEmpty() ? item.id : item.label;
        if (on) {
            emit statusMessage(QStringLiteral("Loop ON: %1").arg(name));
        } else if (wasActive) {
            emit statusMessage(QStringLiteral("Loop OFF: %1").arg(name));
        } else {
            emit statusMessage(QStringLiteral("Loop failed (no actions): %1").arg(name));
        }
        return;
    }

    const QVector<LayoutAction> acts = item.effectiveActions();
    if (acts.isEmpty()) {
        GAZER_WARN << "No actions on item" << item.id;
        return;
    }
    dispatchAll(acts, sourceInstanceId);
}

void ActionDispatcher::dispatchAll(const QVector<LayoutAction>& actions,
                                   const QString& sourceInstanceId)
{
    if (actions.isEmpty()) {
        return;
    }

    // Synchronous fast path when no delays.
    bool anyDelay = false;
    for (const LayoutAction& a : actions) {
        if (a.delayMs > 0) {
            anyDelay = true;
            break;
        }
    }
    if (!anyDelay) {
        for (const LayoutAction& a : actions) {
            dispatchOne(a, sourceInstanceId);
        }
        return;
    }

    struct State {
        QVector<LayoutAction> acts;
        QString sourceId;
        int index = 0;
    };
    auto state = std::make_shared<State>();
    state->acts = actions;
    state->sourceId = sourceInstanceId;

    auto runner = std::make_shared<std::function<void()>>();
    *runner = [this, state, runner]() {
        if (state->index >= state->acts.size()) {
            return;
        }
        // Drop remaining delayed steps if the source board was closed.
        if (!state->sourceId.isEmpty() && !m_svc.instances().instance(state->sourceId)) {
            GAZER_WARN << "Action series aborted — instance gone" << state->sourceId;
            return;
        }
        const LayoutAction act = state->acts.at(state->index);
        ++state->index;
        dispatchOne(act, state->sourceId);
        if (state->index >= state->acts.size()) {
            return;
        }
        if (!state->sourceId.isEmpty() && !m_svc.instances().instance(state->sourceId)) {
            return;
        }
        const int delay = std::max(0, state->acts.at(state->index).delayMs);
        if (delay > 0) {
            QTimer::singleShot(delay, this, [runner]() { (*runner)(); });
        } else {
            (*runner)();
        }
    };

    const int firstDelay = std::max(0, actions.first().delayMs);
    if (firstDelay > 0) {
        QTimer::singleShot(firstDelay, this, [runner]() { (*runner)(); });
    } else {
        (*runner)();
    }
}

void ActionDispatcher::dispatchOne(const LayoutAction& action, const QString& sourceInstanceId,
                                   const QString& itemId)
{
    const auto type = action.type;
    const QString text = action.text;
    const QString layoutId = action.layoutId;
    const QString commandName = action.name;
    const QString script = action.source;

    auto notify = [this](const QString& msg) { emit statusMessage(msg); };

    switch (type) {
    case LayoutAction::Type::Speak: {
        GAZER_INFO << "[speak]" << text;
        QString err;
        if (!m_svc.phrases().speak(text, &err) && !err.isEmpty()) {
            notify(err);
        } else {
            notify(QStringLiteral("Said: %1").arg(text));
        }
        break;
    }

    case LayoutAction::Type::TypeText: {
        if (text.isEmpty()) {
            break;
        }
        GAZER_INFO << "[typeText]" << text;
        InputOutput o;
        o.type = InputOutput::Type::Text;
        o.value = text;
        QString err;
        if (!m_svc.input().execute(o, &err)) {
            notify(err.isEmpty() ? QStringLiteral("typeText failed") : err);
        } else {
            notify(QStringLiteral("Typed: %1").arg(text));
        }
        break;
    }

    case LayoutAction::Type::LoadLayout: {
        QString err;
        if (!applyLoadLayout(m_svc.instances(), sourceInstanceId, layoutId, &err)) {
            notify(err.isEmpty() ? QStringLiteral("loadLayout failed") : err);
        } else {
            notify(QStringLiteral("Loaded: %1").arg(layoutId));
        }
        break;
    }

    case LayoutAction::Type::OpenLayout: {
        QString err;
        const QString id = m_svc.instances().openInstance(layoutId, &err);
        if (id.isEmpty()) {
            notify(QStringLiteral("openLayout failed: %1").arg(err));
        } else {
            notify(QStringLiteral("Opened: %1").arg(layoutId));
        }
        break;
    }

    case LayoutAction::Type::CloseLayout: {
        QString err;
        if (!m_svc.instances().closeInstance(sourceInstanceId, &err)) {
            notify(err);
        } else {
            notify(QStringLiteral("Closed board"));
        }
        break;
    }

    case LayoutAction::Type::Command: {
        QString err;
        if (!m_svc.commands().run({commandName, layoutId, sourceInstanceId}, &err)) {
            notify(err.isEmpty() ? QStringLiteral("Command failed: %1").arg(commandName) : err);
        }
        break;
    }

    case LayoutAction::Type::Script: {
        QString err;
        if (!m_svc.scripts().evaluate(script, &err)) {
            notify(err);
        } else {
            notify(QStringLiteral("Script ok"));
        }
        break;
    }

    case LayoutAction::Type::Unknown:
        GAZER_WARN << "Unknown action on item" << itemId;
        break;
    }
}

} // namespace gazer
