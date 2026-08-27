#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

class QProcess;

namespace gazer {

/// Finds a local AutoHotkey install and runs embedded page-script source in a new process.
class AhkLauncher final : public QObject {
    Q_OBJECT

public:
    explicit AhkLauncher(QObject* parent = nullptr);
    ~AhkLauncher() override;

    /// Empty source is a no-op success. Does not wait for the script to finish.
    [[nodiscard]] bool run(const QString& source, QString* error = nullptr);

    /// Discover AutoHotkey for this source (cached while the file still exists).
    [[nodiscard]] QString findExecutable(const QString& source, QString* error = nullptr);

    /// Rank existing paths. Prefers v2 unless the source `#Requires` v1.
    [[nodiscard]] static QString findExecutableIn(const QStringList& candidates,
                                                  const QString& source);

    /// When set, skip system discovery (`empty` pretends AHK is missing). Test hook.
    void setOverrideExecutable(const std::optional<QString>& path);
    /// Write the temp `.ahk` but do not spawn. Test hook.
    void setDryRun(bool on) { m_dryRun = on; }
    [[nodiscard]] QString lastScriptPath() const { return m_lastScriptPath; }

private:
    [[nodiscard]] QString resolveExecutable(const QString& source, QString* error);
    [[nodiscard]] static QStringList systemCandidates();
    [[nodiscard]] static bool wantsV1(const QString& source);
    void forgetProcess(QProcess* proc, const QString& scriptPath);

    std::optional<QString> m_overrideExe;
    QString m_cachedExe;
    bool m_cachedWantV1 = false;
    bool m_haveCache = false;
    bool m_dryRun = false;
    int m_seq = 0;
    QString m_lastScriptPath;
};

} // namespace gazer
