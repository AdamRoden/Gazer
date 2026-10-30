#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechEngine.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/ScrollBar.h"
#include "ui/Theme.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using compose_detail::kOverlayScrollTrackId;

namespace {

constexpr int kTileCols = 6;
constexpr int kTileRows = 3;
constexpr int kTileCount = kTileCols * kTileRows;
constexpr int kFileNameMax = 180;
constexpr int kHistoryCap = 40;

struct PathChip {
    QString label;
    QString path;
};

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

QString browserEntryLabel(const QString& path)
{
    const QFileInfo info(path);
    if (info.fileName().isEmpty() || info.isRoot()) {
        QString root = info.absoluteFilePath();
        while (root.endsWith(QLatin1Char('/')) || root.endsWith(QLatin1Char('\\'))) {
            root.chop(1);
        }
        return root.isEmpty() ? path : root;
    }
    return info.fileName();
}

QString fileIcon(const QFileInfo& info)
{
    if (info.isRoot()) {
        return QStringLiteral("hardDrive");
    }
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    const QString ext = info.suffix().toLower();
    if (ext == QLatin1String("png") || ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")
        || ext == QLatin1String("gif") || ext == QLatin1String("bmp") || ext == QLatin1String("webp")
        || ext == QLatin1String("svg")) {
        return QStringLiteral("photoCamera");
    }
    if (ext == QLatin1String("wav") || ext == QLatin1String("mp3") || ext == QLatin1String("ogg")
        || ext == QLatin1String("flac") || ext == QLatin1String("m4a") || ext == QLatin1String("aac")) {
        return QStringLiteral("musicNote");
    }
    if (ext == QLatin1String("txt") || ext == QLatin1String("md") || ext == QLatin1String("log")) {
        return QStringLiteral("book");
    }
    if (ext == QLatin1String("json")) {
        return QStringLiteral("editSquare");
    }
    return QStringLiteral("description");
}

bool fileNameCharOk(QChar c)
{
    if (c.unicode() < 32) {
        return false;
    }
    return QStringLiteral("\\/:*?\"<>|").indexOf(c) < 0;
}

QVector<PathChip> pathChips(const QString& dir)
{
    QVector<PathChip> chips;
    if (dir.isEmpty()) {
        chips.push_back({QStringLiteral("This PC"), QString()});
        return chips;
    }
    const QString clean = QDir::cleanPath(dir);
    if (clean.startsWith(QLatin1String("//"))) {
        const QStringList parts = clean.mid(2).split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString acc = QStringLiteral("//");
        for (int i = 0; i < parts.size(); ++i) {
            if (i == 0) {
                acc += parts.at(i);
            } else {
                acc += QLatin1Char('/') + parts.at(i);
            }
            chips.push_back({parts.at(i), acc});
        }
        if (chips.isEmpty()) {
            chips.push_back({QStringLiteral("This PC"), QString()});
        }
        return chips;
    }
    const QStringList parts = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString acc;
    for (int i = 0; i < parts.size(); ++i) {
        if (i == 0) {
            acc = parts.at(i);
        } else {
            acc += QLatin1Char('/') + parts.at(i);
        }
        QString path = acc;
        if (i == 0 && path.size() == 2 && path.at(1) == QLatin1Char(':')) {
            path += QLatin1Char('/');
        }
        chips.push_back({parts.at(i), QDir::cleanPath(path)});
    }
    if (chips.isEmpty()) {
        chips.push_back({clean, clean});
    }
    return chips;
}

// Nearest row start. The short last row is a stop, and a tie keeps it.
int nearestFileOffset(int offset, int count)
{
    const int maxOff = qMax(0, count - kTileCount);
    offset = qBound(0, offset, maxOff);
    const int down = offset - offset % kTileCols;
    const int up = qMin(maxOff, down == offset ? down : down + kTileCols);
    return offset - down < up - offset ? down : up;
}

int stepFileOffset(int offset, int rows, int count)
{
    const int maxOff = qMax(0, count - kTileCount);
    offset = qBound(0, offset, maxOff);
    while (rows > 0 && offset < maxOff) {
        offset = qMin(maxOff, offset - offset % kTileCols + kTileCols);
        --rows;
    }
    while (rows < 0 && offset > 0) {
        const int rem = offset % kTileCols;
        offset -= rem == 0 ? kTileCols : rem;
        ++rows;
    }
    return offset;
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

PageCell navButton(const QString& id, const QString& label, const QString& icon, int col,
                   const QString& command, const QColor& surface)
{
    PageCell c = cell(id, label, 0, col, command, surface, 1, command.isEmpty() ? QStringLiteral("label")
                                                                                : QString(),
                      {}, icon);
    return c;
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

bool ComposeUi::openFiles(QString* error)
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
    const QVector<int> priorLayers =
        firstOpen && isOpen() ? m_pages.attachedCopy(QString(kPageId)).showLayers : QVector<int>{};
    if (m_browserDir.isEmpty()) {
        m_browserDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (m_fileName.text().trimmed().isEmpty()) {
        m_fileName.load(QStringLiteral("aac-workspace-%1.json")
                            .arg(QDateTime::currentDateTime().toString(
                                QStringLiteral("yyyy-MM-dd-HHmmss"))));
    }
    m_fileOffset = 0;
    m_browserFresh = false;
    dismissBoardNote();
    m_fileConfirm = FileConfirm::None;
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

bool ComposeUi::exportBoard(QString* error)
{
    return openFiles(error);
}

bool ComposeUi::importChoose(QString* error)
{
    return openFiles(error);
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
    const QVector<PathChip> chips = pathChips(m_browserDir);
    if (index < 0 || index >= chips.size()) {
        return false;
    }
    navigateBrowser(chips.at(index).path);
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
        return fail(QStringLiteral("Open a folder, then Export"));
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
    const int maxOff = qMax(0, m_browserEntries.size() - kTileCount);
    int offset = qBound(0, m_fileOffset + delta * kTileCount, maxOff);
    m_fileOffset = offset == maxOff ? offset : offset - offset % kTileCols;
    rebuildFiles();
}

void ComposeUi::scrollFileRows(int rows)
{
    if (rows == 0 || !hasLive(kFilesLiveId)) {
        return;
    }
    ensureBrowserEntries();
    const int next = stepFileOffset(m_fileOffset, rows, m_browserEntries.size());
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
    const int next = nearestFileOffset(offset, m_browserEntries.size());
    if (next == m_fileOffset) {
        return;
    }
    m_fileOffset = next;
    rebuildFiles();
}

void ComposeUi::rebuildFiles()
{
    if (!hasLive(kFilesLiveId)) {
        return;
    }
    ensureBrowserEntries();
    m_fileOffset = nearestFileOffset(m_fileOffset, m_browserEntries.size());
    QString err;
    (void)presentLive(QString(kFilesLiveId), buildFilesDocument(), &err);
}

PageDocument ComposeUi::buildFilesDocument() const
{
    PageDocument doc;
    doc.id = QString(kFilesLiveId);
    doc.name = QStringLiteral("File Explorer");
    const ThemeColors theme = m_settings.resolvedTheme();
    // Sit above the Speak keyboard (row weights 1+3+4 of 12, margin 16, gap 8).
    initTopOverlay(doc, 1, 3, {1.15, 6.2, 1.4}, 8, 12, theme,
                   QStringLiteral("(A_ScreenHeight-56)*2/3+24"), speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);

    ensureBrowserEntries();
    const QVector<PathChip> chips = pathChips(m_browserDir);

    PageGrid nav;
    nav.id = QStringLiteral("files_nav");
    nav.nested = true;
    nav.row = 0;
    nav.col = 0;
    nav.rows = 1;
    nav.columns = 3 + chips.size();
    nav.gapPx = 8;
    nav.cells.push_back(navButton(QStringLiteral("file_back"), QStringLiteral("Back"),
                                  QStringLiteral("ArrowLeft"), 0,
                                  m_fileBack.isEmpty() ? QString() : QStringLiteral("compose.browserPrev"),
                                  surface));
    nav.cells.push_back(navButton(QStringLiteral("file_forward"), QStringLiteral("Forward"),
                                  QStringLiteral("ArrowRight"), 1,
                                  m_fileForward.isEmpty() ? QString()
                                                          : QStringLiteral("compose.browserForward"),
                                  surface));
    nav.cells.push_back(navButton(QStringLiteral("file_up"), QStringLiteral("Up"),
                                  QStringLiteral("ArrowUp"), 2,
                                  m_browserDir.isEmpty() ? QString() : QStringLiteral("compose.browserUp"),
                                  surface));
    for (int i = 0; i < chips.size(); ++i) {
        const bool current = i + 1 == chips.size();
        const QString command = current ? QString() : QStringLiteral("compose.browserGoto.%1").arg(i);
        PageCell chip = cell(QStringLiteral("file_chip_%1").arg(i), ellipsis(chips.at(i).label, 18),
                             0, 3 + i, command, current ? accent : surface, 1,
                             current ? QStringLiteral("label") : QString());
        nav.cells.push_back(std::move(chip));
    }
    grid.subGrids.push_back(std::move(nav));

    PageGrid mid;
    mid.id = QStringLiteral("files_mid");
    mid.nested = true;
    mid.row = 1;
    mid.col = 0;
    mid.rows = 1;
    mid.columns = 7;
    mid.gapPx = 8;

    PageGrid tiles;
    tiles.id = QStringLiteral("files_tiles");
    tiles.nested = true;
    tiles.row = 0;
    tiles.col = 0;
    tiles.colSpan = 6;
    tiles.rows = kTileRows;
    tiles.columns = kTileCols;
    tiles.gapPx = 8;
    if (m_browserEntries.isEmpty()) {
        PageCell empty;
        empty.id = QStringLiteral("file_empty");
        empty.row = 0;
        empty.col = 0;
        empty.colSpan = kTileCols;
        empty.role = QStringLiteral("label");
        empty.label = m_browserDir.isEmpty()
                          ? QStringLiteral("No drives")
                          : (QFileInfo(m_browserDir).isDir() ? QStringLiteral("No JSON files")
                                                            : QStringLiteral("Cannot open this folder"));
        tiles.cells.push_back(std::move(empty));
    } else {
        for (int slot = 0; slot < kTileCount; ++slot) {
            const int index = m_fileOffset + slot;
            if (index >= m_browserEntries.size()) {
                break;
            }
            const QString path = m_browserEntries.at(index);
            const QFileInfo info(path);
            const bool dir = info.isDir();
            const QString selected = selectedJsonPath();
            const bool on = !dir && !selected.isEmpty()
                            && QDir::cleanPath(path).compare(QDir::cleanPath(selected),
                                                             Qt::CaseInsensitive) == 0;
            PageCell entry = cell(QStringLiteral("file_entry_%1").arg(slot),
                                  ellipsis(browserEntryLabel(path), 22), slot / kTileCols,
                                  slot % kTileCols, QStringLiteral("compose.importPick.%1").arg(index),
                                  on ? accent : surface, 1, {}, {}, fileIcon(info));
            tiles.cells.push_back(std::move(entry));
        }
    }
    mid.subGrids.push_back(std::move(tiles));

    const int maxOff = qMax(0, m_browserEntries.size() - kTileCount);
    auto rowButton = [&](const char* id, int row, int dir, const char* icon) {
        const bool on = dir < 0 ? m_fileOffset > 0 : m_fileOffset < maxOff;
        return cell(QString::fromLatin1(id), {}, row, 0,
                    on ? QStringLiteral("compose.fileRow.%1").arg(dir) : QString(), surface, 1,
                    on ? QString() : QStringLiteral("label"), {}, QString::fromLatin1(icon));
    };
    PageGrid scroll;
    scroll.id = QStringLiteral("files_scroll");
    scroll.nested = true;
    scroll.row = 0;
    scroll.col = 6;
    scroll.rows = 3;
    scroll.columns = 1;
    scroll.gapPx = 6;
    scroll.marginPx = 4;
    scroll.rowTracks = {PageTrackSize::fromDim(PageDim::pixels(96)), PageTrackSize::starWeight(),
                        PageTrackSize::fromDim(PageDim::pixels(96))};
    scroll.style.background = surface;
    scroll.style.thickness = PageBox::all(0);
    scroll.style.radius = PageBox::all(16);
    scroll.cells.push_back(rowButton("file_row_up", 0, -1, "ArrowUp"));
    PageCell track = cell(QString(kOverlayScrollTrackId), {}, 1, 0, {}, QColor(), 1,
                          QStringLiteral("scrollbar"));
    track.caption = ScrollBar::formatSpec(m_fileOffset, kTileCount, m_browserEntries.size());
    track.style.thickness = PageBox::all(0);
    track.style.radius = PageBox::all(10);
    scroll.cells.push_back(std::move(track));
    scroll.cells.push_back(rowButton("file_row_down", 2, 1, "ArrowDown"));
    mid.subGrids.push_back(std::move(scroll));
    grid.subGrids.push_back(std::move(mid));

    PageGrid foot;
    foot.id = QStringLiteral("files_foot");
    foot.nested = true;
    foot.row = 2;
    foot.col = 0;
    foot.rows = 1;
    foot.columns = 5;
    foot.gapPx = 8;
    foot.columnTracks = starTracks({4.0, 1.3, 1.3, 1.3, 1.3});
    PageCell name;
    name.id = QStringLiteral("file_name");
    name.row = 0;
    name.col = 0;
    name.role = QStringLiteral("display");
    name.textStyle = QStringLiteral("body");
    name.label = m_fileName.text();
    name.caretIndex = qBound(0, m_fileName.caret(), name.label.size());
    if (m_fileName.hasSelection()) {
        name.selectionStart = m_fileName.selectionStart();
        name.selectionEnd = m_fileName.selectionEnd();
    }
    name.caption = m_fileConfirm == FileConfirm::None
                       ? (m_boardNote.isEmpty() ? QStringLiteral("File name") : m_boardNote)
                       : fileConfirmQuestion();
    name.style.background = value;
    name.style.foreground = ThemeColors::contrastOn(value);
    foot.cells.push_back(std::move(name));
    if (m_fileConfirm == FileConfirm::None) {
        foot.cells.push_back(cell(QStringLiteral("file_import"), QStringLiteral("Import"), 0, 1,
                                  QStringLiteral("compose.importReplace"), surface));
        foot.cells.push_back(cell(QStringLiteral("file_merge"), QStringLiteral("Merge"), 0, 2,
                                  QStringLiteral("compose.importMerge"), surface));
        foot.cells.push_back(cell(QStringLiteral("file_export"), QStringLiteral("Export"), 0, 3,
                                  QStringLiteral("compose.browserSave"), accent));
        foot.cells.push_back(cell(QStringLiteral("file_cancel"), QStringLiteral("Cancel"), 0, 4,
                                  QStringLiteral("compose.browserBack"), surface));
    } else {
        foot.cells.push_back(cell(QStringLiteral("file_ask"), fileConfirmQuestion(), 0, 1, {},
                                  surface, 2, QStringLiteral("label")));
        foot.cells.push_back(cell(QStringLiteral("file_yes"), QStringLiteral("Yes"), 0, 3,
                                  QStringLiteral("compose.fileYes"), accent));
        foot.cells.push_back(cell(QStringLiteral("file_no"), QStringLiteral("No"), 0, 4,
                                  QStringLiteral("compose.fileNo"), surface));
    }
    grid.subGrids.push_back(std::move(foot));
    return doc;
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
            return fail(QStringLiteral("Open a folder, then Export"));
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
            return QStringLiteral("Export this file?");
        }
        const bool exists = !m_browserDir.isEmpty() && QFileInfo::exists(QDir(m_browserDir).filePath(name));
        return ellipsis(exists ? QStringLiteral("Overwrite %1?").arg(name)
                               : QStringLiteral("Export %1?").arg(name),
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
