#pragma once

#include "mapping/MappingEngine.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <initializer_list>

namespace gazer {

/// Resolves command names: built-in handlers first, then mapping profile.
class CommandRegistry final : public QObject {
    Q_OBJECT

public:
    struct Invocation {
        QString name;
        QString pageId;
    };

    using Handler = std::function<bool(QString* error)>;
    using InvHandler = std::function<bool(const Invocation&, QString* error)>;

    explicit CommandRegistry(MappingEngine& mapping, QObject* parent = nullptr);

    void registerBuiltin(const QString& name, Handler handler);
    void registerBuiltin(const QString& name, InvHandler handler);
    /// Same handler under several names (canonical first). See `src/app/Commands.md`.
    void registerBuiltin(std::initializer_list<const char*> names, Handler handler);
    void registerBuiltin(std::initializer_list<const char*> names, InvHandler handler);
    /// Longest-prefix match after exact builtins. Prefix should end with '.'.
    void registerPrefix(const QString& prefix, InvHandler handler);
    [[nodiscard]] bool isBuiltin(const QString& name) const;
    [[nodiscard]] QStringList names() const;

    /// Builtin if registered, else mapping profile injectors.
    [[nodiscard]] bool run(const QString& commandName, QString* error = nullptr);
    [[nodiscard]] bool run(const Invocation& inv, QString* error = nullptr);

signals:
    void statusMessage(const QString& message);

private:
    MappingEngine& m_mapping;
    QHash<QString, InvHandler> m_builtins;
    struct PrefixHandler {
        QString prefix;
        InvHandler handler;
    };
    QVector<PrefixHandler> m_prefixes;
};

} // namespace gazer
