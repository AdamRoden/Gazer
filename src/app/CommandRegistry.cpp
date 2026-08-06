#include "app/CommandRegistry.h"

#include "utils/Log.h"

namespace gazer {

CommandRegistry::CommandRegistry(MappingEngine& mapping, QObject* parent)
    : QObject(parent)
    , m_mapping(mapping)
{
}

void CommandRegistry::registerBuiltin(const QString& name, Handler handler)
{
    m_builtins.insert(name, std::move(handler));
}

bool CommandRegistry::isBuiltin(const QString& name) const
{
    return m_builtins.contains(name);
}

bool CommandRegistry::run(const QString& commandName, QString* error)
{
    if (auto it = m_builtins.constFind(commandName); it != m_builtins.constEnd()) {
        GAZER_INFO << "[command builtin]" << commandName;
        const bool ok = (*it)(error);
        if (ok) {
            emit statusMessage(QStringLiteral("Cmd %1").arg(commandName));
        }
        return ok;
    }
    return m_mapping.runCommand(commandName, error);
}

} // namespace gazer
