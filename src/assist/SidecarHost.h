#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

class QProcess;

namespace gazer {

class AhkLauncher;

enum class SidecarKind { Python, Ahk };

struct SidecarRequest {
    SidecarKind kind = SidecarKind::Python;
    QString file;
    QString args;
    QString key;
    bool persist = false;
};

struct SidecarLaunch {
    QString program;
    QStringList arguments;
    QString workDir;
    QString key;
};

/// Spawns Python / AutoHotkey scripts from page `<Run>` actions. One-shot or resident.
class SidecarHost final : public QObject {
    Q_OBJECT

public:
    explicit SidecarHost(QObject* parent = nullptr);
    ~SidecarHost() override;

    void setAhk(AhkLauncher* ahk) { m_ahk = ahk; }
    void setActionPipe(const QString& name) { m_actionPipe = name; }

    /// Extra allowed roots (page directory). AppData and the app `resources` dir are always
    /// allowed. `file` must resolve under one of those after `QDir::cleanPath`.
    [[nodiscard]] bool run(const SidecarRequest& req, const QStringList& extraRoots,
                           QString* error = nullptr);

    [[nodiscard]] static QStringList defaultAllowedRoots();
    [[nodiscard]] static bool pathIsUnder(const QString& absFile, const QString& absRoot);
    [[nodiscard]] static bool resolveScriptPath(const QString& file, const QStringList& roots,
                                                QString* absOut, QString* error = nullptr);

    void setOverridePython(const std::optional<QString>& path);
    void setDryRun(bool on) { m_dryRun = on; }
    [[nodiscard]] SidecarLaunch lastLaunch() const { return m_last; }

private:
    struct Resident {
        QProcess* proc = nullptr;
        SidecarLaunch launch;
    };

    [[nodiscard]] QString resolvePython(bool* pyLauncher, QString* error);
    [[nodiscard]] QString resolveAhk(const QString& scriptPath, QString* error);
    void forgetProcess(QProcess* proc);
    void stopAll();
    static bool scriptExtensionOk(SidecarKind kind, const QString& path, QString* error);

    AhkLauncher* m_ahk = nullptr;
    QString m_actionPipe = QStringLiteral("Gazer");
    std::optional<QString> m_overridePython;
    bool m_dryRun = false;
    QHash<QString, Resident> m_resident;
    SidecarLaunch m_last;
};

} // namespace gazer
