#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace gazer {

bool ComposeUi::writeWorkspaceFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& msg) {
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        if (hasLive(kFilesLiveId) || isOpen()) {
            refresh();
        }
        return false;
    };
    if (capturingPhrase()) {
        return fail(QStringLiteral("Finish the name first"));
    }
    const QFileInfo info(path);
    const QString leaf = info.fileName();
    if (leaf.isEmpty() || leaf == QLatin1String(".") || leaf == QLatin1String("..")) {
        return fail(QStringLiteral("Type a file name"));
    }
    const QString dir = info.absolutePath();
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
        return fail(QStringLiteral("Could not use that folder"));
    }
    SoundboardStore::WorkspaceExportSettings settings;
    settings.model = m_settings.speechModel;
    settings.speed = m_settings.speechSpeed;
    settings.pitch = m_settings.speechPitch;
    settings.volumeStop = qBound(1, qRound(m_settings.speechVolume), 5);
    settings.theme = themeAppearanceIsDark(m_settings.themeAppearance) ? QStringLiteral("dark")
                                                                        : QStringLiteral("light");
    settings.accentColor = m_settings.customPrimaryColor;
    bool omitted = false;
    const QByteArray bytes = m_board.exportWorkspace(settings, &omitted);
    const QString full = info.absoluteFilePath();
    QFile file(full);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        return fail(QStringLiteral("Could not write the export"));
    }
    file.close();
    m_fileName.load(leaf);
    m_browserDir = dir;
    m_browserFresh = false;
    const QString msg = omitted ? QStringLiteral("Saved %1 without audio").arg(full)
                                : QStringLiteral("Saved %1").arg(full);
    setBoardNote(msg, true);
    GAZER_INFO << "Exported workspace" << full;
    if (hasLive(kFilesLiveId) || isOpen()) {
        refresh();
    }
    return true;
}

} // namespace gazer
