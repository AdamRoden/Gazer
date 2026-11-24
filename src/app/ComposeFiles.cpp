#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeFilesDetail.h"
#include "app/ComposeUiInternal.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechEngine.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gazer {

using files_detail::clampFileOffset;
using files_detail::exportLeafName;
using files_detail::filePageSize;
using files_detail::filePlaces;
using files_detail::fileRowStep;
using files_detail::pathChips;

namespace {

constexpr int kFileNameMax = 180;
constexpr int kHistoryCap = 40;

QStringList listBrowser(const QString& dir, const QStringList& patterns)
{
    QStringList out;
    if (dir.isEmpty()) {
        const QFileInfoList drives = QDir::drives();
        for (const QFileInfo& drive : drives) {
            out.push_back(drive.absoluteFilePath());
        }
        return out;
    }
    const QDir folder(dir);
    if (!folder.exists()) {
        return out;
    }
    const QFileInfoList dirs = folder.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& info : dirs) {
        out.push_back(info.absoluteFilePath());
        if (out.size() >= 2000) {
            return out;
        }
    }
    const QFileInfoList files = folder.entryInfoList(
        patterns.isEmpty() ? QStringList{QStringLiteral("*.json")} : patterns,
        QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& info : files) {
        out.push_back(info.absoluteFilePath());
        if (out.size() >= 2000) {
            break;
        }
    }
    return out;
}

bool workspaceFileLooksValid(const QJsonObject& root, QString* error)
{
    const QString format = root.value(QStringLiteral("format")).toString();
    if (!format.isEmpty() && format != QLatin1String("aac-workspace")
        && format != QLatin1String("gazer-soundboard")) {
        if (error) {
            *error = QStringLiteral("Not an AAC Workspace export file");
        }
        return false;
    }
    const QJsonValue topics = root.value(QStringLiteral("topics"));
    if (!topics.isArray() || topics.toArray().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Export has no topics");
        }
        return false;
    }
    return true;
}

} // namespace

bool ComposeUi::editingFileName() const
{
    return m_pages.topPageId() == kFilesLiveId;
}

bool ComposeUi::exportDialogOpen() const
{
    return editingFileName() && m_fileMode == FileMode::Export && browserFace().ownsEnter;
}

ComposeBuffer& ComposeUi::editTarget()
{
    if (textFieldOpen()) {
        return m_fieldBuffer;
    }
    return editingFileName() ? m_fileName : m_buffer;
}

bool ComposeUi::handleFileSend(const PageAction& action)
{
    if (!editingFileName()) {
        return false;
    }
    const QString key = action.sendKey.trimmed();
    if (key.size() == 1) {
        insertFileName(key);
    } else if (key.compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
        insertFileName(QStringLiteral(" "));
    }
    return true;
}

bool ComposeUi::handleFileCommand(const QString& command)
{
    if (!editingFileName()) {
        return false;
    }
    const QString c = command.trimmed();
    if (c == QLatin1String("backspace")) {
        backspace();
    } else if (c == QLatin1String("space")) {
        insertFileName(QStringLiteral(" "));
    } else if (c == QLatin1String("arrowLeft")) {
        nudgeFileCaret(-1);
    } else if (c == QLatin1String("arrowRight")) {
        nudgeFileCaret(1);
    } else if (m_fileMode == FileMode::Export
               && (c == QLatin1String("home") || c == QLatin1String("end"))) {
        useCaretHost(CaretLook::Host::File);
        applyCaretChoice(c == QLatin1String("home") ? PhraseCaretChoice::Home
                                                    : PhraseCaretChoice::End);
        finishCaretKey();
    }
    return true;
}

bool ComposeUi::handleFileEscape()
{
    if (!editingFileName()) {
        return false;
    }
    if (m_fileConfirm != FileConfirm::None) {
        confirmFileNo();
    } else {
        browserBack();
    }
    return true;
}

bool ComposeUi::consumeFileSpeak(QString* error)
{
    Q_UNUSED(error);
    if (!editingFileName()) {
        return false;
    }
    const auto st = m_speech.status();
    if (st.busy || st.speaking) {
        stopSpeech();
    }
    return true;
}

bool ComposeUi::importMerge(QString* error)
{
    return requestFileImport(false, error);
}

bool ComposeUi::importReplace(QString* error)
{
    return requestFileImport(true, error);
}

bool ComposeUi::requestFileImport(bool replace, QString* error)
{
    if (m_fileJob == FileJob::Settings) {
        if (!replace) {
            const QString msg = QStringLiteral("Settings import replaces the file");
            notify(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        if (hasLive(kFilesLiveId)) {
            return armFileConfirm(FileConfirm::Import, error);
        }
        return importSettingsSelection(error);
    }
    if (hasLive(kFilesLiveId)) {
        return armFileConfirm(replace ? FileConfirm::Import : FileConfirm::Merge, error);
    }
    return importPending(replace, error);
}

void ComposeUi::insertFileName(QStringView chars)
{
    QString clean;
    clean.reserve(int(chars.size()));
    for (QChar c : chars) {
        if (files_detail::fileNameCharOk(c)) {
            clean += c;
        }
    }
    if (clean.isEmpty()) {
        return;
    }
    const int room = kFileNameMax - m_fileName.text().size();
    if (room <= 0) {
        return;
    }
    if (clean.size() > room) {
        clean.truncate(room);
    }
    m_fileName.insert(clean);
    commitEdit();
}

void ComposeUi::ensureBrowserEntries() const
{
    if (m_browserFresh) {
        return;
    }
    m_browserEntries = listBrowser(m_browserDir, browserFace().patterns);
    m_browserFresh = true;
}

void ComposeUi::navigateBrowser(const QString& dir)
{
    const QString next = dir.isEmpty() ? QString() : QDir::cleanPath(dir);
    const QString cur = m_browserDir.isEmpty() ? QString() : QDir::cleanPath(m_browserDir);
    if (next.compare(cur, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_fileBack.push_back(m_browserDir);
    if (m_fileBack.size() > kHistoryCap) {
        m_fileBack.removeFirst();
    }
    m_fileForward.clear();
    m_browserDir = next;
    m_fileOffset = 0;
    m_browserFresh = false;
    m_fileConfirm = FileConfirm::None;
    dismissBoardNote();
    rebuildFiles();
}

bool ComposeUi::openFileMode(FileMode mode, QString* error, FileJob job)
{
    if (job == FileJob::HistoryClip
        && (m_historyClipPath.isEmpty() || m_historyClipName.trimmed().isEmpty())) {
        const QString msg = QStringLiteral("No recording to save");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (capturingPhrase() && !files_detail::jobDuringItemEdit(job)) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const bool firstOpen = !hasLive(kFilesLiveId);
    const bool switching = !firstOpen && m_fileMode != mode;
    const bool jobChanged = m_fileJob != job;
    const QVector<int> priorLayers =
        firstOpen && isOpen() ? m_pages.attachedCopy(QString(kPageId)).showLayers : QVector<int>{};
    if (m_browserDir.isEmpty()) {
        m_browserDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    m_fileMode = mode;
    m_fileJob = job;
    if (jobChanged) {
        m_browserFresh = false;
    }
    if (job == FileJob::Audio && (firstOpen || jobChanged)) {
        m_pendingImport.clear();
        m_pendingImportName.clear();
        m_pendingImportPath.clear();
        m_fileName.load(phraseAudioLeaf());
        m_browserFresh = false;
    }
    if (job != FileJob::HistoryClip) {
        m_historyClipPath.clear();
        m_historyClipName.clear();
    }
    if (job != FileJob::Topic) {
        m_topicExportId.clear();
        m_topicExportName.clear();
    }
    if (mode == FileMode::Export && job == FileJob::HistoryClip) {
        m_fileName.load(m_historyClipName.trimmed());
    } else if (mode == FileMode::Export && job == FileJob::Topic) {
        m_fileName.load(m_topicExportName.trimmed());
    } else if (mode == FileMode::Export && (jobChanged || m_fileName.text().trimmed().isEmpty())) {
        if (job == FileJob::Settings) {
            m_fileName.load(QStringLiteral("gazer-settings.json"));
        } else {
            m_fileName.load(QStringLiteral("aac-workspace-%1.json")
                                .arg(QDateTime::currentDateTime().toString(
                                    QStringLiteral("yyyy-MM-dd-HHmmss"))));
        }
    }
    if (firstOpen) {
        m_fileOffset = 0;
        m_browserFresh = false;
        dismissBoardNote();
        m_fileConfirm = FileConfirm::None;
    } else if (switching) {
        dismissBoardNote();
        m_fileConfirm = FileConfirm::None;
    }
    const bool ok = presentLive(QString(kFilesLiveId), buildFilesDocument(), error);
    if (!ok) {
        return false;
    }
    if (firstOpen) {
        m_fileRestoreLayers = priorLayers;
    }
    if (isOpen()) {
        showComposeLayer(1);
    }
    return true;
}

bool ComposeUi::openFiles(QString* error)
{
    return openFileMode(FileMode::Import, error);
}

bool ComposeUi::exportBoard(QString* error)
{
    return openFileMode(FileMode::Export, error);
}

bool ComposeUi::importChoose(QString* error)
{
    return openFileMode(FileMode::Import, error);
}

bool ComposeUi::browserUp(QString* error)
{
    Q_UNUSED(error);
    if (m_browserDir.isEmpty()) {
        return true;
    }
    QDir dir(m_browserDir);
    const QString current = dir.absolutePath();
    if (!dir.cdUp() || dir.absolutePath().compare(current, Qt::CaseInsensitive) == 0) {
        navigateBrowser(QString());
    } else {
        navigateBrowser(dir.absolutePath());
    }
    return true;
}

bool ComposeUi::browserPrev(QString* error)
{
    Q_UNUSED(error);
    if (m_fileBack.isEmpty()) {
        return true;
    }
    m_fileForward.push_back(m_browserDir);
    m_browserDir = m_fileBack.takeLast();
    m_fileOffset = 0;
    m_browserFresh = false;
    m_fileConfirm = FileConfirm::None;
    dismissBoardNote();
    rebuildFiles();
    return true;
}

bool ComposeUi::browserForward(QString* error)
{
    Q_UNUSED(error);
    if (m_fileForward.isEmpty()) {
        return true;
    }
    m_fileBack.push_back(m_browserDir);
    m_browserDir = m_fileForward.takeLast();
    m_fileOffset = 0;
    m_browserFresh = false;
    m_fileConfirm = FileConfirm::None;
    dismissBoardNote();
    rebuildFiles();
    return true;
}

bool ComposeUi::browserGoto(int index, QString* error)
{
    Q_UNUSED(error);
    const QVector<files_detail::PathChip> chips = pathChips(m_browserDir);
    if (index < 0 || index >= chips.size()) {
        return false;
    }
    navigateBrowser(chips.at(index).path);
    return true;
}

bool ComposeUi::browserPlace(int index, QString* error)
{
    Q_UNUSED(error);
    const QVector<files_detail::Place> places = filePlaces();
    if (index < 0 || index >= places.size()) {
        return false;
    }
    navigateBrowser(places.at(index).path);
    return true;
}

bool ComposeUi::browserSave(QString* error)
{
    if (hasLive(kFilesLiveId)) {
        return armFileConfirm(FileConfirm::Export, error);
    }
    return writeExport(error);
}

bool ComposeUi::noteFileFailure(const QString& msg, QString* error)
{
    setBoardNote(msg);
    if (error) {
        *error = msg;
    }
    if (hasLive(kFilesLiveId)) {
        rebuildFiles();
    }
    return false;
}

files_detail::BrowserFace ComposeUi::browserFace() const
{
    const auto mode = m_fileMode == FileMode::Export ? files_detail::BrowserMode::Export
                                                     : files_detail::BrowserMode::Import;
    return files_detail::browserFace(m_fileJob, mode, SoundboardStore::audioNameFilters());
}

QString ComposeUi::exportDestination(QString* error)
{
    if (m_browserDir.isEmpty() || !QFileInfo(m_browserDir).isDir()) {
        noteFileFailure(QStringLiteral("Open a folder, then Save"), error);
        return {};
    }
    QString why;
    const QString name = exportLeafName(m_fileName.text(), &why, browserFace().exportSuffix);
    if (name.isEmpty()) {
        noteFileFailure(why.isEmpty() ? QStringLiteral("Type a file name") : why, error);
        return {};
    }
    return QDir(m_browserDir).filePath(name);
}

bool ComposeUi::writeExport(QString* error)
{
    switch (browserFace().writer) {
    case files_detail::BrowserFace::Writer::Settings:
        return writeSettingsExport(error);
    case files_detail::BrowserFace::Writer::Clip:
        return writeHistoryClip(error);
    case files_detail::BrowserFace::Writer::Topic:
    case files_detail::BrowserFace::Writer::Workspace:
        break;
    }
    const QString path = exportDestination(error);
    if (path.isEmpty()) {
        return false;
    }
    return writeWorkspaceFile(path, error);
}

bool ComposeUi::writeSettingsExport(QString* error)
{
    const QString path = exportDestination(error);
    if (path.isEmpty()) {
        return false;
    }
    if (!m_settingsExport) {
        return noteFileFailure(QStringLiteral("Export is unavailable"), error);
    }
    QString err;
    if (!m_settingsExport(path, &err)) {
        return noteFileFailure(err.isEmpty() ? QStringLiteral("Could not export settings") : err,
                               error);
    }
    browserBack();
    return true;
}

bool ComposeUi::importSettingsSelection(QString* error)
{
    const QString path = selectedJsonPath();
    if (path.isEmpty()) {
        return noteFileFailure(QStringLiteral("Dwell a JSON file first"), error);
    }
    if (!m_settingsImport) {
        return noteFileFailure(QStringLiteral("Import is unavailable"), error);
    }
    QString err;
    if (!m_settingsImport(path, &err)) {
        return noteFileFailure(err.isEmpty() ? QStringLiteral("Could not import settings") : err,
                               error);
    }
    browserBack();
    return true;
}

void ComposeUi::browserBack()
{
    const bool settingsJob = m_fileJob == FileJob::Settings;
    m_fileConfirm = FileConfirm::None;
    m_fileJob = FileJob::Speak;
    m_historyClipPath.clear();
    m_historyClipName.clear();
    m_topicExportId.clear();
    m_topicExportName.clear();
    const bool wasOpen = hasLive(kFilesLiveId);
    if (wasOpen) {
        m_pages.closePage(QString(kFilesLiveId));
    }
    if (!m_boardNoteKeep) {
        dismissBoardNote();
    }
    if (wasOpen && isOpen() && !settingsJob) {
        restoreFileLayers();
    } else {
        m_fileRestoreLayers.clear();
    }
    if (settingsJob) {
        releaseBorrowedKeyboard();
        return;
    }
    refresh();
}

bool ComposeUi::pickImportFile(int index, QString* error)
{
    if (capturingPhrase() && !browserFace().duringItemEdit) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    ensureBrowserEntries();
    if (index < 0 || index >= m_browserEntries.size()) {
        const QString msg = browserFace().dwellHint;
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        rebuildFiles();
        return false;
    }
    const QString path = m_browserEntries.at(index);
    const QFileInfo info(path);
    if (info.isDir()) {
        navigateBrowser(info.absoluteFilePath());
        return true;
    }
    m_fileConfirm = FileConfirm::None;
    m_pendingImport.clear();
    m_fileName.load(info.fileName());
    m_pendingImportName = info.fileName();
    m_pendingImportPath = info.absoluteFilePath();
    setBoardNote(QStringLiteral("Selected %1").arg(info.fileName()));
    rebuildFiles();
    return true;
}

void ComposeUi::pageImportFiles(int delta)
{
    if (delta == 0) {
        return;
    }
    ensureBrowserEntries();
    m_fileOffset = clampFileOffset(m_fileOffset + delta * filePageSize(m_fileTiles),
                                   m_browserEntries.size(), m_fileTiles);
    rebuildFiles();
}

void ComposeUi::scrollFileRows(int rows)
{
    if (rows == 0 || !hasLive(kFilesLiveId)) {
        return;
    }
    ensureBrowserEntries();
    const int next = clampFileOffset(m_fileOffset + rows * fileRowStep(m_fileTiles),
                                     m_browserEntries.size(), m_fileTiles);
    if (next == m_fileOffset) {
        return;
    }
    m_fileOffset = next;
    rebuildFiles();
}

void ComposeUi::filesGoto(int offset)
{
    if (!hasLive(kFilesLiveId)) {
        return;
    }
    ensureBrowserEntries();
    const int next = clampFileOffset(offset, m_browserEntries.size(), m_fileTiles);
    if (next == m_fileOffset) {
        return;
    }
    m_fileOffset = next;
    rebuildFiles();
}

void ComposeUi::setFileTiles(bool tiles)
{
    if (m_fileTiles == tiles) {
        return;
    }
    m_fileTiles = tiles;
    rebuildFiles();
}

void ComposeUi::rebuildFiles()
{
    if (!hasLive(kFilesLiveId)) {
        return;
    }
    ensureBrowserEntries();
    m_fileOffset = clampFileOffset(m_fileOffset, m_browserEntries.size(), m_fileTiles);
    QString err;
    (void)presentLive(QString(kFilesLiveId), buildFilesDocument(), &err);
}

bool ComposeUi::armFileConfirm(FileConfirm kind, QString* error)
{
    auto fail = [&](const QString& msg) {
        m_fileConfirm = FileConfirm::None;
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        rebuildFiles();
        return false;
    };
    if (capturingPhrase() && !browserFace().duringItemEdit) {
        return fail(QStringLiteral("Finish the name first"));
    }
    if (m_fileJob == FileJob::Audio) {
        if (selectedAudioPath().isEmpty()) {
            return fail(QStringLiteral("Dwell an audio file first"));
        }
    } else if (kind == FileConfirm::Export) {
        if (m_browserDir.isEmpty() || !QFileInfo(m_browserDir).isDir()) {
            return fail(QStringLiteral("Open a folder, then Save"));
        }
        QString why;
        if (exportLeafName(m_fileName.text(), &why, browserFace().exportSuffix).isEmpty()) {
            return fail(why.isEmpty() ? QStringLiteral("Type a file name") : why);
        }
    } else if (selectedJsonPath().isEmpty()) {
        return fail(QStringLiteral("Dwell a JSON file first"));
    }
    m_fileConfirm = kind;
    rebuildFiles();
    return true;
}

bool ComposeUi::confirmFileYes(QString* error)
{
    const FileConfirm kind = m_fileConfirm;
    m_fileConfirm = FileConfirm::None;
    if (kind == FileConfirm::None) {
        return true;
    }
    if (kind == FileConfirm::Export) {
        if (m_fileJob == FileJob::Settings) {
            return writeSettingsExport(error);
        }
        return writeExport(error);
    }
    if (m_fileJob == FileJob::Audio) {
        return rememberPhraseAudio(error);
    }
    if (m_fileJob == FileJob::Settings) {
        return importSettingsSelection(error);
    }
    if (!readSelectedWorkspace(error)) {
        if (hasLive(kFilesLiveId)) {
            rebuildFiles();
        }
        return false;
    }
    return importPending(kind == FileConfirm::Import, error);
}

void ComposeUi::confirmFileNo()
{
    if (m_fileConfirm == FileConfirm::None) {
        return;
    }
    m_fileConfirm = FileConfirm::None;
    rebuildFiles();
}

bool ComposeUi::loadPendingFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& msg) {
        m_pendingImport.clear();
        m_pendingImportName.clear();
        m_pendingImportPath.clear();
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        return false;
    };
    if (path.isEmpty()) {
        return fail(QStringLiteral("Dwell a JSON file first"));
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Could not read the file"));
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        return fail(QStringLiteral("Invalid file"));
    }
    QString invalid;
    if (!workspaceFileLooksValid(json.object(), &invalid)) {
        return fail(invalid.isEmpty() ? QStringLiteral("Invalid file") : invalid);
    }
    m_pendingImport = json.toJson(QJsonDocument::Compact);
    m_pendingImportName = QFileInfo(path).fileName();
    m_pendingImportPath = path;
    return true;
}

bool ComposeUi::readSelectedWorkspace(QString* error)
{
    return loadPendingFile(selectedJsonPath(), error);
}

QString ComposeUi::selectedJsonPath() const
{
    return selectedTypedFile(QStringLiteral(".json"));
}

QString ComposeUi::selectedTypedFile(const QString& suffix) const
{
    const QString typed = m_fileName.text().trimmed();
    if (!typed.isEmpty()) {
        return files_detail::fileInDir(m_browserDir, typed, suffix);
    }
    const QFileInfo saved(m_pendingImportPath);
    const QString want = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
    if (saved.isFile() && saved.suffix().compare(want, Qt::CaseInsensitive) == 0) {
        return saved.absoluteFilePath();
    }
    return {};
}

QString ComposeUi::selectedBrowserFile() const
{
    const files_detail::BrowserFace face = browserFace();
    if (face.selection == files_detail::BrowserFace::Selection::Audio) {
        return selectedAudioPath();
    }
    return selectedTypedFile(face.exportSuffix);
}

QString ComposeUi::selectedAudioPath() const
{
    const QFileInfo saved(m_pendingImportPath);
    if (saved.isFile() && !SoundboardStore::storedAudioSuffix(saved.suffix()).isEmpty()) {
        return saved.absoluteFilePath();
    }
    return {};
}

bool ComposeUi::browsePhraseAudio(QString* error)
{
    if (m_nameEdit != NameEditKind::Pin || m_editId.isEmpty()) {
        const QString msg = QStringLiteral("Open a phrase first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    closeFieldEditor();
    auto folderOf = [](const QString& path) {
        if (path.isEmpty()) {
            return QString();
        }
        const QFileInfo info(path);
        const QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        return QDir(dir).exists() ? QDir(dir).absolutePath() : QString();
    };
    QString start = folderOf(m_formAudioPath);
    if (start.isEmpty() && !m_formAudioClear) {
        if (const SoundboardButton* b = m_board.findButton(m_editId); b && !b->clipId.isEmpty()) {
            const QString path = m_board.clipPath(b->clipId);
            if (QFileInfo::exists(path)) {
                start = folderOf(path);
            }
        }
    }
    if (!start.isEmpty()) {
        m_browserDir = start;
        m_fileOffset = 0;
        m_browserFresh = false;
    }
    return openFileMode(FileMode::Import, error, FileJob::Audio);
}

bool ComposeUi::clearPhraseAudio(QString* error)
{
    if (m_nameEdit != NameEditKind::Pin || m_editId.isEmpty()) {
        return noteFileFailure(QStringLiteral("Open a phrase first"), error);
    }
    m_formAudioPath.clear();
    m_formAudioName.clear();
    m_formAudioClear = true;
    if (hasLive(kFilesLiveId) && m_fileJob == FileJob::Audio) {
        browserBack();
    }
    rebuildForm();
    return true;
}

bool ComposeUi::usePhraseAudio(QString* error)
{
    if (!hasLive(kFilesLiveId) || m_fileJob != FileJob::Audio) {
        const QString msg = QStringLiteral("Open a phrase first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    return armFileConfirm(FileConfirm::Import, error);
}

bool ComposeUi::rememberPhraseAudio(QString* error)
{
    if (m_nameEdit != NameEditKind::Pin || m_editId.isEmpty()) {
        return noteFileFailure(QStringLiteral("Open a phrase first"), error);
    }
    const QString path = selectedAudioPath();
    if (path.isEmpty()) {
        return noteFileFailure(QStringLiteral("Dwell an audio file first"), error);
    }
    m_formAudioClear = false;
    m_formAudioPath = path;
    m_formAudioName = QFileInfo(path).fileName();
    browserBack();
    rebuildForm();
    notify(QStringLiteral("Audio selected"));
    return true;
}

QString ComposeUi::fileConfirmQuestion() const
{
    if (m_fileJob == FileJob::Audio && m_fileConfirm != FileConfirm::None) {
        const QString path = selectedAudioPath();
        const QString name = path.isEmpty() ? QStringLiteral("this file") : QFileInfo(path).fileName();
        return ellipsis(QStringLiteral("Assign %1?").arg(name), 72);
    }
    if (m_fileConfirm == FileConfirm::Export) {
        const QString name = exportLeafName(m_fileName.text(), nullptr, browserFace().exportSuffix);
        if (name.isEmpty()) {
            return QStringLiteral("Save this file?");
        }
        const bool exists = !m_browserDir.isEmpty() && QFileInfo::exists(QDir(m_browserDir).filePath(name));
        return ellipsis(exists ? QStringLiteral("Overwrite %1?").arg(name)
                               : QStringLiteral("Save %1?").arg(name),
                        72);
    }
    const QString path = selectedJsonPath();
    const QString name = path.isEmpty() ? QStringLiteral("this file") : QFileInfo(path).fileName();
    const QString verb = m_fileConfirm == FileConfirm::Merge ? QStringLiteral("Merge")
                                                            : QStringLiteral("Import");
    return ellipsis(QStringLiteral("%1 %2?").arg(verb, name), 72);
}

void ComposeUi::restoreFileLayers()
{
    const QVector<int> layers = m_fileRestoreLayers;
    m_fileRestoreLayers.clear();
    if (layers.isEmpty() || layers == QVector<int>{1}) {
        return;
    }
    PageAction action;
    action.type = PageActionType::ShowLayers;
    action.layers = layers;
    QString err;
    (void)m_pages.showLayers({action}, QString(kPageId), {}, &err);
}

} // namespace gazer
