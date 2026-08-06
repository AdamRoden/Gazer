#include "mapping/MappingEngine.h"

#include "mapping/MappingLoader.h"
#include "utils/Log.h"

namespace gazer {

MappingEngine::MappingEngine(InputService& input, QObject* parent)
    : QObject(parent)
    , m_input(input)
{
}

bool MappingEngine::loadProfileFile(const QString& path, QString* error)
{
    MappingProfile p;
    if (!MappingLoader::loadFromFile(path, p, error)) {
        return false;
    }
    m_profile = std::move(p);
    emit statusMessage(QStringLiteral("Mapping: %1").arg(m_profile.name));
    return true;
}

bool MappingEngine::runCommand(const QString& commandName, QString* error)
{
    if (!m_profile.isValid()) {
        if (error) {
            *error = QStringLiteral("No mapping profile loaded");
        }
        return false;
    }
    if (!m_profile.commands.contains(commandName)) {
        if (error) {
            *error = QStringLiteral("Unmapped command: %1").arg(commandName);
        }
        GAZER_WARN << "Unmapped command:" << commandName;
        return false;
    }
    const QVector<InputOutput> outs = m_profile.outputsForCommand(commandName);
    if (outs.isEmpty()) {
        GAZER_INFO << "Command has empty output list:" << commandName;
        return true;
    }
    QString err;
    if (!m_input.executeAll(outs, &err)) {
        if (error) {
            *error = err;
        }
        return false;
    }
    emit statusMessage(QStringLiteral("Cmd %1").arg(commandName));
    return true;
}

} // namespace gazer

