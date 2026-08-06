#include "app/ActionDispatcher.h"

#include "input/InputTypes.h"
#include "layout/SessionNavigate.h"
#include "utils/Log.h"

namespace gazer {

ActionDispatcher::ActionDispatcher(GazerServices& services, QObject* parent)
    : QObject(parent)
    , m_svc(services)
{
}

void ActionDispatcher::dispatch(LayoutItem item, const QString& sourceInstanceId)
{
    const auto type = item.action.type;
    const QString text = item.action.text;
    const QString layoutId = item.action.layoutId;
    const QString commandName = item.action.name;
    const QString script = item.action.source;
    const QString itemId = item.id;

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
        if (!m_svc.commands().run(commandName, &err)) {
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
