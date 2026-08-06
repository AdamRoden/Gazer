#pragma once

#include "input/InputTypes.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace gazer {

struct MappingProfile {
    int schemaVersion = 1;
    QString id;
    QString name;
    /// command name → ordered outputs
    QHash<QString, QVector<InputOutput>> commands;
    /// When true, speak actions also type the phrase via keyboard.
    bool speakAlsoType = false;

    [[nodiscard]] bool isValid() const { return !id.isEmpty(); }

    [[nodiscard]] QVector<InputOutput> outputsForCommand(const QString& command) const
    {
        return commands.value(command);
    }
};

} // namespace gazer
