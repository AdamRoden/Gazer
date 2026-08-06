#pragma once

#include "mapping/MappingEngine.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

namespace gazer {

/// Resolves layout command names: built-in handlers first, then mapping profile.
class CommandRegistry final : public QObject {
    Q_OBJECT

public:
    using Handler = std::function<bool(QString* error)>;

    explicit CommandRegistry(MappingEngine& mapping, QObject* parent = nullptr);

    void registerBuiltin(const QString& name, Handler handler);
    [[nodiscard]] bool isBuiltin(const QString& name) const;

    /// Builtin if registered, else mapping profile injectors.
    [[nodiscard]] bool run(const QString& commandName, QString* error = nullptr);

signals:
    void statusMessage(const QString& message);

private:
    MappingEngine& m_mapping;
    QHash<QString, Handler> m_builtins;
};

} // namespace gazer
