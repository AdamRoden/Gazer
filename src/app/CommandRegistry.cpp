#include "app/CommandRegistry.h"

#include "utils/Log.h"

#include <QStringList>
#include <QVector>

namespace gazer {

CommandRegistry::CommandRegistry(MappingEngine& mapping, QObject* parent)
    : QObject(parent)
    , m_mapping(mapping)
{
}

void CommandRegistry::registerBuiltin(const QString& name, Handler handler)
{
    m_builtins.insert(name, [h = std::move(handler)](const Invocation&, QString* error) {
        return h(error);
    });
}

void CommandRegistry::registerBuiltin(const QString& name, InvHandler handler)
{
    m_builtins.insert(name, std::move(handler));
}

void CommandRegistry::registerBuiltin(std::initializer_list<const char*> names, Handler handler)
{
    for (const char* n : names) {
        registerBuiltin(QLatin1String(n), handler);
    }
}

void CommandRegistry::registerPrefix(const QString& prefix, InvHandler handler)
{
    m_prefixes.push_back(PrefixHandler{prefix, std::move(handler)});
}

QStringList CommandRegistry::names() const
{
    QStringList n = m_builtins.keys();
    n.append(m_mapping.profile().commands.keys());
    n.removeDuplicates();
    n.sort(Qt::CaseInsensitive);
    return n;
}

bool CommandRegistry::run(const QString& commandName, QString* error)
{
    return run(Invocation{commandName, {}}, error);
}

bool CommandRegistry::run(const Invocation& inv, QString* error)
{
    const InvHandler* handler = nullptr;
    if (auto it = m_builtins.constFind(inv.name); it != m_builtins.constEnd()) {
        handler = &(*it);
    } else {
        int bestLen = -1;
        for (const PrefixHandler& p : m_prefixes) {
            if (inv.name.startsWith(p.prefix) && int(p.prefix.size()) > bestLen) {
                bestLen = int(p.prefix.size());
                handler = &p.handler;
            }
        }
    }
    if (!handler) {
        return m_mapping.runCommand(inv.name, error);
    }
    GAZER_INFO << "[command builtin]" << inv.name;
    const bool ok = (*handler)(inv, error);
    const bool skipToast = inv.name.startsWith(QLatin1String("compose."))
                           || inv.name.startsWith(QLatin1String("speech."))
                           || inv.name.startsWith(QLatin1String("soundboard."))
                           || inv.name.startsWith(QLatin1String("history."))
                           || inv.name.startsWith(QLatin1String("settings."))
                           || inv.name.startsWith(QLatin1String("headPose."))
                           || inv.name.startsWith(QLatin1String("lookTo."));
    if (ok && !skipToast) {
        emit statusMessage(QStringLiteral("Cmd %1").arg(inv.name));
    }
    return ok;
}

} // namespace gazer
