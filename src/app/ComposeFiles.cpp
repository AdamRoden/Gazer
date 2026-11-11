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
using files_detail::filePageSize;
using files_detail::filePlaces;
using files_detail::fileRowStep;
using files_detail::pathChips;

namespace {

constexpr int kFileNameMax = 180;
constexpr int kHistoryCap = 40;

QStringList listBrowser(const QString& dir)
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
        QStringList{QStringLiteral("*.json")}, QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& info : files) {
        out.push_back(info.absoluteFilePath());
        if (out.size() >= 2000) {
            break;
        }
    }
    return out;
}

bool fileNameCharOk(QChar c)
{
    if (c.unicode() < 32) {
        return false;
    }
    return QStringLiteral("\\/:*?\"<>|").indexOf(c) < 0;
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

bool reservedFileName(const QString& stem)
{
    const QString s = stem.toUpper();
    if (s == QLatin1String("CON") || s == QLatin1String("PRN") || s == QLatin1String("AUX")
        || s == QLatin1String("NUL")) {
        return true;
    }
    if (s.startsWith(QLatin1String("COM")) || s.startsWith(QLatin1String("LPT"))) {
        bool digits = s.size() > 3;
        for (int i = 3; digits && i < s.size(); ++i) {
            digits = s.at(i).isDigit();
        }
        if (digits) {
            return true;
        }
    }
    return false;
}

QString exportLeafName(const QString& typed, QString* error)
{
    const QString msg = QStringLiteral("Type a file name");
    const QString name = typed.trimmed();
    if (name.isEmpty() || name.endsWith(QLatin1Char('.'))) {
        if (error) {
            *error = msg;
        }
        return {};
    }
    QString leaf = name;
    if (!leaf.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        leaf += QStringLiteral(".json");
    }
    const QString stem = QFileInfo(leaf).completeBaseName();
    bool bad = reservedFileName(stem);
    for (int i = 0; !bad && i < leaf.size(); ++i) {
        bad = !fileNameCharOk(leaf.at(i));
    }
    if (bad) {
        if (error) {
            *error = msg;
        }
        return {};
    }
    return leaf;
}

QString jsonInDir(const QString& dir, const QString& typed)
{
    if (dir.isEmpty()) {
        return {};
    }
    QString name = typed.trimmed();
    if (name.isEmpty() || name.endsWith(QLatin1Char('.'))) {
        return {};
    }
    if (!name.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        name += QStringLiteral(".json");
    }
    for (const QChar c : name) {
        if (!fileNameCharOk(c)) {
            return {};
        }
    }
    const QFileInfo info(QDir(dir).filePath(name));
    if (!info.isFile() || info.suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    return info.absoluteFilePath();
}

} // namespace

bool ComposeUi::editingFileName() const
{
    return m_pages.topPageId() == kFilesLiveId;
}

ComposeBuffer& ComposeUi::editTarget()
{
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
        if (fileNameCharOk(c)) {
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
    m_browserEntries = listBrowser(m_browserDir);
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

bool ComposeUi::openFileMode(FileMode mode, QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const bool firstOpen = !hasLive(kFilesLiveId);
    const bool switching = !firstOpen && m_fileMode != mode;
    const QVector<int> priorLayers =
        firstOpen && isOpen() ? m_pages.attachedCopy(QString(kPageId)).showLayers : QVector<int>{};
    if (m_browserDir.isEmpty()) {
        m_browserDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    m_fileMode = mode;
    if (mode == FileMode::Export && m_fileName.text().trimmed().isEmpty()) {
        m_fileName.load(QStringLiteral("aac-workspace-%1.json")
                            .arg(QDateTime::currentDateTime().toString(
                                QStringLiteral("yyyy-MM-dd-HHmmss"))));
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

void ComposeUi::nudgeFileCaret(int delta)
{
    if (delta == 0 || !hasLive(kFilesLiveId) || m_fileMode != FileMode::Export) {
        return;
    }
    const int next = qBound(0, m_fileName.caret() + delta, m_fileName.text().size());
    if (next == m_fileName.caret() && !m_fileName.hasSelection()) {
        return;
    }
    m_fileName.setCaret(next);
    rebuildFiles();
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

bool ComposeUi::writeExport(QString* error)
{
    auto fail = [&](const QString& msg) {
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        if (hasLive(kFilesLiveId)) {
            rebuildFiles();
        }
        return false;
    };
    if (capturingPhrase()) {
        return fail(QStringLiteral("Finish the name first"));
    }
    if (m_browserDir.isEmpty() || !QFileInfo(m_browserDir).isDir()) {
        return fail(QStringLiteral("Open a folder, then Save"));
    }
    QString why;
    const QString name = exportLeafName(m_fileName.text(), &why);
    if (name.isEmpty()) {
        return fail(why.isEmpty() ? QStringLiteral("Type a file name") : why);
    }
    return writeWorkspaceFile(QDir(m_browserDir).filePath(name), error);
}

void ComposeUi::browserBack()
{
    m_fileConfirm = FileConfirm::None;
    const bool wasOpen = hasLive(kFilesLiveId);
    if (wasOpen) {
        m_pages.closePage(QString(kFilesLiveId));
    }
    if (!m_boardNoteKeep) {
        dismissBoardNote();
    }
    if (wasOpen && isOpen()) {
        restoreFileLayers();
    } else {
        m_fileRestoreLayers.clear();
    }
    refresh();
}

bool ComposeUi::pickImportFile(int index, QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    ensureBrowserEntries();
    if (index < 0 || index >= m_browserEntries.size()) {
        const QString msg = QStringLiteral("Dwell a folder or a JSON file");
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
    if (capturingPhrase()) {
        return fail(QStringLiteral("Finish the name first"));
    }
    if (kind == FileConfirm::Export) {
        if (m_browserDir.isEmpty() || !QFileInfo(m_browserDir).isDir()) {
            return fail(QStringLiteral("Open a folder, then Save"));
        }
        QString why;
        if (exportLeafName(m_fileName.text(), &why).isEmpty()) {
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
        return writeExport(error);
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
    const QString typed = m_fileName.text().trimmed();
    if (!typed.isEmpty()) {
        return jsonInDir(m_browserDir, typed);
    }
    const QFileInfo saved(m_pendingImportPath);
    if (saved.isFile() && saved.suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0) {
        return saved.absoluteFilePath();
    }
    return {};
}

QString ComposeUi::fileConfirmQuestion() const
{
    if (m_fileConfirm == FileConfirm::Export) {
        const QString name = exportLeafName(m_fileName.text(), nullptr);
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
