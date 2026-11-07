#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeFilesDetail.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QDate>
#include <QFileInfo>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using files_detail::Place;
using files_detail::browserEntryLabel;
using files_detail::filePlaces;
using files_detail::kFileRows;
using files_detail::pathChips;
using files_detail::samePath;

namespace {

constexpr int kFileRowPx = 62;

PageTrackSize pxTrack(int px)
{
    return PageTrackSize::fromDim(PageDim::pixels(px));
}

QVector<PageTrackSize> detailColumns()
{
    return {PageTrackSize::starWeight(), pxTrack(128), pxTrack(96), pxTrack(80)};
}

QColor pickedFill(const QColor& surface, const QColor& accent)
{
    QColor fill = ThemeColors::mix(surface, accent, 0.45);
    fill.setAlpha(255);
    return fill;
}

void framePane(PageGrid& grid, const ThemeColors& theme)
{
    grid.style.borderColor = theme.border.isValid() ? theme.border : QColor(42, 44, 46);
    grid.style.thickness = PageBox::all(1);
    grid.style.radius = PageBox::all(12);
}

QString fileIcon(const QFileInfo& info)
{
    if (info.isRoot()) {
        return QStringLiteral("hardDrive");
    }
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    if (info.suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("editSquare");
    }
    return QStringLiteral("description");
}

QString fileWhen(const QFileInfo& info)
{
    if (info.isRoot() || !info.exists()) {
        return {};
    }
    const QDate day = info.lastModified().date();
    const QDate today = QDate::currentDate();
    if (day == today) {
        return QStringLiteral("Today");
    }
    if (day == today.addDays(-1)) {
        return QStringLiteral("Yesterday");
    }
    if (day.year() == today.year()) {
        return day.toString(QStringLiteral("d MMM"));
    }
    return day.toString(QStringLiteral("d MMM yyyy"));
}

QString fileBytes(qint64 bytes)
{
    if (bytes < 0) {
        return {};
    }
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg((bytes + 512) / 1024);
    }
    const double mb = double(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(QString::number(mb, 'f', mb >= 10.0 ? 0 : 1));
}

struct FileFacts {
    QString when;
    QString kind;
    QString size;
};

FileFacts fileFacts(const QFileInfo& info)
{
    FileFacts f;
    f.kind = QStringLiteral("JSON");
    if (info.isRoot()) {
        f.kind = QStringLiteral("Disk");
    } else if (info.isDir()) {
        f.kind = QStringLiteral("Folder");
    } else {
        f.size = fileBytes(info.size());
    }
    f.when = fileWhen(info);
    return f;
}

PageCell navButton(const QString& id, const QString& label, const QString& icon, int col,
                   const QString& command, const QColor& surface)
{
    return cell(id, label, 0, col, command, surface, 1, {}, {}, icon);
}

PageCell columnHead(const QString& id, const QString& text, int col, const QColor& ink)
{
    PageCell c = cell(id, text, 0, col, {}, QColor(0, 0, 0, 0), 1, QStringLiteral("label"));
    c.textStyle = QStringLiteral("caption");
    c.style.thickness = PageBox::all(0);
    c.style.radius = PageBox::all(0);
    if (ink.isValid()) {
        c.style.foreground = ink;
    }
    return c;
}

PageCell columnCell(const QString& id, const QString& text, int col, const QString& command,
                    const QString& icon)
{
    PageCell c = cell(id, text, 0, col, command, QColor(0, 0, 0, 0), 1, {}, {}, icon);
    c.textStyle = QStringLiteral("row");
    c.style.thickness = PageBox::all(0);
    c.style.radius = PageBox::all(0);
    return c;
}

} // namespace

PageDocument ComposeUi::buildFilesDocument() const
{
    PageDocument doc;
    doc.id = QString(kFilesLiveId);
    doc.name = QStringLiteral("File Explorer");
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 1, 4, {1.0, 1.0, 6.0, 1.2}, 8, 12, theme,
                   QStringLiteral("(A_ScreenHeight-56)*2/3+24"), speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {pxTrack(56), pxTrack(64), PageTrackSize::starWeight(), pxTrack(80)};
    const QColor surface = theme.bgAt(90);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(138, 180, 248);
    const QColor picked = pickedFill(surface, accent);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QColor clear(0, 0, 0, 0);
    const QColor ink = theme.textSecondary.isValid() ? theme.textSecondary : theme.text;

    ensureBrowserEntries();
    const QVector<files_detail::PathChip> chips = pathChips(m_browserDir);

    const bool atPc = m_browserDir.isEmpty();
    const QFileInfo here(m_browserDir);
    const QString titleText = atPc ? QStringLiteral("This PC") : browserEntryLabel(m_browserDir);
    const QString titleIcon =
        (atPc || here.isRoot()) ? QStringLiteral("hardDrive") : QStringLiteral("folder");

    PageGrid titleBar;
    titleBar.id = QStringLiteral("files_title");
    titleBar.nested = true;
    titleBar.row = 0;
    titleBar.col = 0;
    titleBar.rows = 1;
    titleBar.columns = 2;
    titleBar.gapPx = 8;
    titleBar.columnTracks = {PageTrackSize::starWeight(), pxTrack(132)};
    PageCell title = cell(QStringLiteral("file_title"), titleText, 0, 0, {}, clear, 1,
                          QStringLiteral("label"), {}, titleIcon);
    title.textStyle = QStringLiteral("row");
    title.style.thickness = PageBox::all(0);
    titleBar.cells.push_back(std::move(title));
    PageCell close = cell(QStringLiteral("file_close"), QStringLiteral("Close"), 0, 1,
                          QStringLiteral("compose.browserBack"), surface, 1, {}, {},
                          QStringLiteral("close"));
    close.textStyle = QStringLiteral("row");
    titleBar.cells.push_back(std::move(close));
    grid.subGrids.push_back(std::move(titleBar));

    PageGrid bar;
    bar.id = QStringLiteral("files_nav");
    bar.nested = true;
    bar.row = 1;
    bar.col = 0;
    bar.rows = 1;
    bar.columns = 4;
    bar.gapPx = 8;
    bar.columnTracks = {pxTrack(100), pxTrack(112), pxTrack(88), PageTrackSize::starWeight()};
    bar.cells.push_back(navButton(QStringLiteral("file_back"), QStringLiteral("Back"),
                                  QStringLiteral("ArrowLeft"), 0,
                                  QStringLiteral("compose.browserPrev"), surface));
    bar.cells.push_back(navButton(QStringLiteral("file_forward"), QStringLiteral("Forward"),
                                  QStringLiteral("ArrowRight"), 1,
                                  QStringLiteral("compose.browserForward"), surface));
    bar.cells.push_back(navButton(QStringLiteral("file_up"), QStringLiteral("Up"),
                                  QStringLiteral("ArrowUp"), 2, QStringLiteral("compose.browserUp"),
                                  surface));
    PageGrid address;
    address.id = QStringLiteral("files_address");
    address.nested = true;
    address.row = 0;
    address.col = 3;
    address.rows = 1;
    address.columns = qMax(1, chips.size());
    address.gapPx = 4;
    address.marginPx = 6;
    address.style.background = value;
    address.style.borderColor = theme.border.isValid() ? theme.border : QColor(42, 44, 46);
    address.style.thickness = PageBox::all(1);
    address.style.radius = PageBox::all(8);
    for (int i = 0; i < chips.size(); ++i) {
        const bool current = i + 1 == chips.size();
        const QString command =
            current ? QString() : QStringLiteral("compose.browserGoto.%1").arg(i);
        PageCell chip = cell(QStringLiteral("file_chip_%1").arg(i), chips.at(i).label, 0, i,
                             command, current ? picked : clear, 1,
                             current ? QStringLiteral("label") : QString());
        chip.textStyle = QStringLiteral("row");
        if (!current) {
            chip.style.thickness = PageBox::all(0);
        }
        address.cells.push_back(std::move(chip));
    }
    bar.subGrids.push_back(std::move(address));
    grid.subGrids.push_back(std::move(bar));

    PageGrid body;
    body.id = QStringLiteral("files_body");
    body.nested = true;
    body.row = 2;
    body.col = 0;
    body.rows = 1;
    body.columns = 2;
    body.gapPx = 8;
    body.columnTracks = {pxTrack(248), PageTrackSize::starWeight()};

    const QVector<Place> places = filePlaces();
    PageGrid side;
    side.id = QStringLiteral("files_places");
    side.nested = true;
    side.row = 0;
    side.col = 0;
    side.rows = places.size() + 1;
    side.columns = 1;
    side.gapPx = 6;
    side.marginPx = 8;
    framePane(side, theme);
    side.rowTracks.push_back(pxTrack(22));
    for (int i = 0; i < places.size(); ++i) {
        side.rowTracks.push_back(PageTrackSize::starWeight());
    }
    PageCell placesLabel;
    placesLabel.id = QStringLiteral("file_places_label");
    placesLabel.role = QStringLiteral("label");
    placesLabel.textStyle = QStringLiteral("caption");
    placesLabel.label = QStringLiteral("Places");
    placesLabel.style.background = clear;
    placesLabel.style.thickness = PageBox::all(0);
    side.cells.push_back(std::move(placesLabel));
    for (int i = 0; i < places.size(); ++i) {
        const Place& place = places.at(i);
        const bool on = samePath(place.path, m_browserDir);
        PageCell entry = cell(QStringLiteral("file_place_%1").arg(i), place.label, i + 1, 0,
                              on ? QString() : QStringLiteral("compose.browserPlace.%1").arg(i),
                              on ? picked : surface, 1, on ? QStringLiteral("label") : QString(),
                              {}, place.icon);
        entry.textStyle = QStringLiteral("row");
        side.cells.push_back(std::move(entry));
    }
    body.subGrids.push_back(std::move(side));

    PageGrid main;
    main.id = QStringLiteral("files_main");
    main.nested = true;
    main.row = 0;
    main.col = 1;
    main.rows = 2;
    main.columns = 2;
    main.gapPx = 6;
    main.marginPx = 8;
    main.columnTracks = {PageTrackSize::starWeight(), pxTrack(compose_detail::kScrollChromePx)};
    main.rowTracks = {pxTrack(26), PageTrackSize::starWeight()};
    framePane(main, theme);

    PageGrid head;
    head.id = QStringLiteral("file_cols");
    head.nested = true;
    head.row = 0;
    head.col = 0;
    head.rows = 1;
    head.columns = 4;
    head.gapPx = 0;
    head.columnTracks = detailColumns();
    head.cells.push_back(columnHead(QStringLiteral("file_col_name"), QStringLiteral("Name"), 0, ink));
    head.cells.push_back(
        columnHead(QStringLiteral("file_col_when"), QStringLiteral("Modified"), 1, ink));
    head.cells.push_back(columnHead(QStringLiteral("file_col_type"), QStringLiteral("Type"), 2, ink));
    head.cells.push_back(columnHead(QStringLiteral("file_col_size"), QStringLiteral("Size"), 3, ink));
    main.subGrids.push_back(std::move(head));

    const QString selected = selectedJsonPath();
    PageGrid list;
    list.id = QStringLiteral("files_list");
    list.nested = true;
    list.row = 1;
    list.col = 0;
    list.columns = 1;
    list.gapPx = 6;
    if (m_browserEntries.isEmpty()) {
        list.rows = 1;
        list.rowTracks = {PageTrackSize::starWeight()};
        PageCell empty;
        empty.id = QStringLiteral("file_empty");
        empty.role = QStringLiteral("label");
        empty.textStyle = QStringLiteral("row");
        empty.label = atPc ? QStringLiteral("No drives")
                           : (here.isDir() ? QStringLiteral("No folders or JSON files")
                                           : QStringLiteral("Cannot open this folder"));
        empty.style.background = clear;
        empty.style.thickness = PageBox::all(0);
        list.cells.push_back(std::move(empty));
    } else {
        const int shown = qMin(kFileRows, m_browserEntries.size() - m_fileOffset);
        list.rows = shown + 1;
        for (int slot = 0; slot < shown; ++slot) {
            list.rowTracks.push_back(pxTrack(kFileRowPx));
            const int index = m_fileOffset + slot;
            const QString path = m_browserEntries.at(index);
            const QFileInfo info(path);
            const bool on = !info.isDir() && !selected.isEmpty() && samePath(path, selected);
            const FileFacts facts = fileFacts(info);
            const QString command = QStringLiteral("compose.importPick.%1").arg(index);
            PageGrid row;
            row.id = QStringLiteral("file_row_%1").arg(slot);
            row.nested = true;
            row.row = slot;
            row.col = 0;
            row.rows = 1;
            row.columns = 4;
            row.gapPx = 0;
            row.columnTracks = detailColumns();
            row.style.background = on ? picked : surface;
            row.style.thickness = PageBox::all(0);
            row.style.radius = PageBox::all(10);
            row.cells.push_back(columnCell(QStringLiteral("file_entry_%1").arg(slot),
                                           browserEntryLabel(path), 0, command, fileIcon(info)));
            row.cells.push_back(columnCell(QStringLiteral("file_when_%1").arg(slot), facts.when, 1,
                                           command, {}));
            row.cells.push_back(columnCell(QStringLiteral("file_kind_%1").arg(slot), facts.kind, 2,
                                           command, {}));
            row.cells.push_back(columnCell(QStringLiteral("file_size_%1").arg(slot), facts.size, 3,
                                           command, {}));
            list.subGrids.push_back(std::move(row));
        }
        list.rowTracks.push_back(PageTrackSize::starWeight());
    }
    main.subGrids.push_back(std::move(list));

    PageGrid scroll = compose_detail::overlayScrollbar(
        m_fileOffset, kFileRows, m_browserEntries.size(), surface,
        QStringLiteral("compose.fileRow.-1"), QStringLiteral("compose.fileRow.1"));
    scroll.id = QStringLiteral("files_scroll");
    scroll.row = 1;
    scroll.col = 1;
    main.subGrids.push_back(std::move(scroll));
    body.subGrids.push_back(std::move(main));
    grid.subGrids.push_back(std::move(body));

    PageGrid foot;
    foot.id = QStringLiteral("files_foot");
    foot.nested = true;
    foot.row = 3;
    foot.col = 0;
    foot.rows = 1;
    foot.columns = 2;
    foot.gapPx = 8;
    foot.columnTracks = {pxTrack(248), PageTrackSize::starWeight()};
    const int count = m_browserEntries.size();
    PageCell items = cell(QStringLiteral("file_count"),
                          count == 1 ? QStringLiteral("1 item")
                                     : QStringLiteral("%1 items").arg(count),
                          0, 0, {}, surface, 1, QStringLiteral("label"));
    items.textStyle = QStringLiteral("row");
    foot.cells.push_back(std::move(items));

    PageGrid actions;
    actions.id = QStringLiteral("files_actions");
    actions.nested = true;
    actions.row = 0;
    actions.col = 1;
    actions.rows = 1;
    actions.columns = 4;
    actions.gapPx = 8;
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
    actions.cells.push_back(std::move(name));
    if (m_fileConfirm == FileConfirm::None) {
        actions.columnTracks = starTracks({2.6, 1.15, 1.15, 1.25});
        actions.cells.push_back(cell(QStringLiteral("file_import"), QStringLiteral("Import"), 0, 1,
                                     QStringLiteral("compose.importReplace"), surface));
        actions.cells.push_back(cell(QStringLiteral("file_merge"), QStringLiteral("Merge"), 0, 2,
                                     QStringLiteral("compose.importMerge"), surface));
        actions.cells.push_back(cell(QStringLiteral("file_export"), QStringLiteral("Export"), 0, 3,
                                     QStringLiteral("compose.browserSave"), accent));
    } else {
        actions.columnTracks = starTracks({2.2, 2.0, 1.15, 1.15});
        actions.cells.push_back(cell(QStringLiteral("file_ask"), fileConfirmQuestion(), 0, 1, {},
                                     surface, 1, QStringLiteral("label")));
        actions.cells.push_back(cell(QStringLiteral("file_yes"), QStringLiteral("Yes"), 0, 2,
                                     QStringLiteral("compose.fileYes"), accent));
        actions.cells.push_back(cell(QStringLiteral("file_no"), QStringLiteral("No"), 0, 3,
                                     QStringLiteral("compose.fileNo"), surface));
    }
    foot.subGrids.push_back(std::move(actions));
    grid.subGrids.push_back(std::move(foot));
    return doc;
}

} // namespace gazer
