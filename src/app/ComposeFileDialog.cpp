#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/OsFileDialog.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace gazer {

namespace {

OsFileRequest workspaceFileRequest(OsFileRequest::Kind kind, const QString& title,
                                   const QString& directory, const QString& fileName)
{
    OsFileRequest req;
    req.kind = kind;
    req.title = title;
    req.directory = directory;
    req.fileName = fileName;
    req.defaultExtension = QStringLiteral("json");
    req.types.push_back(OsFileType{QStringLiteral("JSON"), QStringLiteral("*.json")});
    req.types.push_back(OsFileType{QStringLiteral("All files"), QStringLiteral("*.*")});
    return req;
}

QString workspaceStartDir(const QString& browserDir)
{
    if (!browserDir.isEmpty()) {
        return browserDir;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

} // namespace

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

bool ComposeUi::importFromDialog(QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (m_pages.exclusivePageActive()) {
        return true;
    }
    runOsFileDialog(
        m_pages, workspaceFileRequest(OsFileRequest::Kind::Open, QStringLiteral("Import boards"),
                                      workspaceStartDir(m_browserDir), {}),
        [this](const QString& path, const QString& why) {
            if (path.isEmpty()) {
                if (!why.isEmpty()) {
                    setBoardNote(why);
                }
                return;
            }
            m_browserDir = QFileInfo(path).absolutePath();
            if (!loadPendingFile(path, nullptr)) {
                return;
            }
            importPending(true, nullptr);
        });
    return true;
}

bool ComposeUi::exportToDialog(QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (m_pages.exclusivePageActive()) {
        return true;
    }
    QString name = m_fileName.text().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("aac-workspace-%1.json")
                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmmss")));
    } else if (!name.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        name += QStringLiteral(".json");
    }
    runOsFileDialog(
        m_pages, workspaceFileRequest(OsFileRequest::Kind::Save, QStringLiteral("Export boards"),
                                      workspaceStartDir(m_browserDir), name),
        [this](const QString& path, const QString& why) {
            if (path.isEmpty()) {
                if (!why.isEmpty()) {
                    setBoardNote(why);
                }
                return;
            }
            writeWorkspaceFile(path, nullptr);
        });
    return true;
}

} // namespace gazer
