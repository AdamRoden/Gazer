#include "app/CommandRegistry.h"

#include "utils/Log.h"

#include <QStringList>

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

bool CommandRegistry::isBuiltin(const QString& name) const
{
    return m_builtins.contains(name);
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
    if (auto it = m_builtins.constFind(inv.name); it != m_builtins.constEnd()) {
        GAZER_INFO << "[command builtin]" << inv.name;
        const bool ok = (*it)(inv, error);
        if (ok) {
            emit statusMessage(QStringLiteral("Cmd %1").arg(inv.name));
        }
        return ok;
    }
    return m_mapping.runCommand(inv.name, error);
}

} // namespace gazer
