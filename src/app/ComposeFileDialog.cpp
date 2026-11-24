#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeFilesDetail.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace gazer {

bool ComposeUi::exportTopic(QString* error)
{
    if (m_nameEdit != NameEditKind::Topic || m_editId.isEmpty()) {
        return noteFileFailure(QStringLiteral("Open a topic first"), error);
    }
    const SoundboardTopic* topic = m_board.findTopic(m_editId);
    if (!topic) {
        return noteFileFailure(QStringLiteral("That topic is gone"), error);
    }
    const QString topicId = topic->id;
    const QString topicName = topic->name;
    saveFormEditor();
    m_topicExportId = topicId;
    m_topicExportName = files_detail::exportStem(topicName, QStringLiteral("gazer-topic"))
                        + QStringLiteral(".json");
    if (!openFileMode(FileMode::Export, error, FileJob::Topic)) {
        m_topicExportId.clear();
        m_topicExportName.clear();
        return false;
    }
    return true;
}

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
    const bool oneTopic = m_fileJob == FileJob::Topic;
    const QString onlyTopicId = oneTopic ? m_topicExportId : QString();
    if (oneTopic && (onlyTopicId.isEmpty() || m_board.findTopic(onlyTopicId) == nullptr)) {
        return fail(QStringLiteral("That topic is gone"));
    }
    if (capturingPhrase() && !oneTopic) {
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
    const QByteArray bytes = m_board.exportWorkspace(settings, &omitted,
                                                     SoundboardStore::kWorkspaceMaxBytes, onlyTopicId);
    if (bytes.isEmpty()) {
        return fail(oneTopic ? QStringLiteral("That topic is gone")
                             : QStringLiteral("Could not write the export"));
    }
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
    if (oneTopic) {
        GAZER_INFO << "Exported topic" << onlyTopicId << full;
    } else {
        GAZER_INFO << "Exported workspace" << full;
    }
    if (hasLive(kFilesLiveId) || isOpen()) {
        refresh();
    }
    return true;
}

} // namespace gazer
