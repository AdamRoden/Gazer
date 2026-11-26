#pragma once

#include "app/ComposeUiInternal.h"
#include "utils/ScreenGrab.h"

#include <QVector>

namespace gazer::compose_detail {

/// Sheet chrome for the style and tag lists. The row tracks and the visible
/// row count both come from these sizes.
constexpr int kNameSheetMargin = 16;
constexpr int kNameSheetGap = 8;
constexpr int kNameTitlePx = 72;
constexpr int kNameFooterPx = 72;

struct NameSheetMetrics {
    PageTrackSize keyRow;
    int nameRows = 1;
    bool naming = false;

    [[nodiscard]] QVector<PageTrackSize> rowTracks() const
    {
        QVector<PageTrackSize> tracks;
        tracks.push_back(trackPx(kNameTitlePx));
        tracks.push_back(PageTrackSize::starWeight());
        tracks.push_back(keyRow);
        if (naming) {
            tracks.push_back(trackPx(kNameFooterPx));
        }
        return tracks;
    }
};

inline NameSheetMetrics nameSheetMetrics(const PageDocument& authored, bool naming)
{
    NameSheetMetrics metrics;
    metrics.naming = naming;
    metrics.keyRow = keyboardRowTrack(authored);
    const QRect screen = overlayScreenGeometry();
    const double screenH = qMax(1, screen.height());
    const double screenW = qMax(1, screen.width());
    const double sheetH = qMax(0.0, PageDim::expression(aboveKeyboardHeight(authored))
                                         .resolve(screenH, screenH, screenW, screenH));
    const double keyRow = qMax(1.0, PageDim::expression(keyboardRowExpr(authored))
                                        .resolve(screenH, screenH, screenW, screenH));
    const int sheetRows = metrics.rowTracks().size();
    const double inner = sheetH - 2.0 * kNameSheetMargin - kNameSheetGap * (sheetRows - 1);
    double fixed = double(kNameTitlePx) + keyRow;
    if (naming) {
        fixed += kNameFooterPx;
    }
    const double body = inner - fixed;
    if (body < keyRow) {
        metrics.nameRows = 1;
        return metrics;
    }
    int rows = int((body + kNameListGap) / (keyRow + kNameListGap));
    while (rows > 1 && rows * keyRow + (rows - 1) * kNameListGap > body + 0.5) {
        --rows;
    }
    metrics.nameRows = qMax(1, rows);
    return metrics;
}

inline int nameListRows(const PageDocument& authored, bool naming)
{
    return nameSheetMetrics(authored, naming).nameRows;
}

/// One name on the style or tag sheet. An empty `favCommand` is a plain tile
/// (style None): no star and no delete.
struct NameEntry {
    QString label;
    QString command;
    QString favCommand;
    QString trashCommand;
    bool favorite = false;
    bool selected = false;
};

struct NameSheet {
    QString id;
    QString name;
    QString title;
    QString prefix;
    bool naming = false;
    QVector<NameEntry> entries;
    int* page = nullptr;
    QString pageUp;
    QString pageDown;
    QString searchId;
    QString query;
    QString placeholder;
    QString clearCommand;
    QString addCommand;
    int caret = 0;
    bool stampCaret = false;
};

inline PageDocument buildNameSheet(const NameSheet& spec, const PageDocument& authored,
                                   const ThemeColors& theme, const QString& heightExpr, PageDim width)
{
    PageDocument doc;
    doc.id = spec.id;
    doc.name = spec.name;
    const NameSheetMetrics metrics = nameSheetMetrics(authored, spec.naming);
    const int sheetRows = metrics.rowTracks().size();
    SettingsPageBuild::initTopOverlay(doc, 1, sheetRows, QVector<double>(sheetRows, 1.0),
                                      kNameSheetGap, kNameSheetMargin, theme, heightExpr,
                                      std::move(width));
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = metrics.rowTracks();

    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    grid.subGrids.push_back(speakTitleBar(spec.title, spec.naming, key, false));

    const int total = spec.entries.size();
    const int rows = metrics.nameRows;
    const int page = namePageSize(rows);
    const int maxOff = qMax(0, total - page);
    if (spec.page) {
        *spec.page = qBound(0, *spec.page, maxOff);
    }
    const int start = spec.page ? *spec.page : 0;

    PageGrid body = overlayBody(1, 1);
    PageGrid list = overlayList(kNameCols, rows);
    list.gapPx = kNameListGap;
    list.rowTracks = QVector<PageTrackSize>(rows, metrics.keyRow);
    for (int slot = 0; slot < page; ++slot) {
        const int index = start + slot;
        const int tileRow = slot / kNameCols;
        const int tileCol = slot % kNameCols;
        const QString id = QStringLiteral("%1_%2").arg(spec.prefix).arg(slot);
        if (index >= total) {
            PageCell empty = SettingsPageBuild::cell(id, {}, tileRow, tileCol, {}, QColor(), 1,
                                                     QStringLiteral("label"));
            paintFlat(empty);
            list.cells.push_back(std::move(empty));
            continue;
        }
        const NameEntry& entry = spec.entries.at(index);
        if (entry.favCommand.isEmpty()) {
            PageCell plain = SettingsPageBuild::cell(id, entry.label, tileRow, tileCol, entry.command,
                                                     key);
            plain.style.radius = PageBox::all(12);
            if (entry.selected) {
                paintSelected(plain, accent, key);
            } else {
                paintSurface(plain, key);
            }
            list.cells.push_back(std::move(plain));
            continue;
        }
        list.subGrids.push_back(nameCard(id, tileRow, tileCol, entry.label, entry.command,
                                         entry.favCommand, entry.favorite, entry.trashCommand,
                                         entry.selected, value, accent, key, danger));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, page, total, key, spec.pageUp, spec.pageDown));
    grid.subGrids.push_back(std::move(body));

    if (spec.naming) {
        PageGrid field = searchBar(spec.searchId, spec.query, spec.placeholder, 2, key, accent,
                                   spec.clearCommand, false);
        if (spec.stampCaret && !field.subGrids.isEmpty() && !field.subGrids[0].cells.isEmpty()) {
            PageCell& text = field.subGrids[0].cells[0];
            text.caretIndex = spec.query.isEmpty() ? 0 : qBound(0, spec.caret, text.label.size());
        }
        grid.subGrids.push_back(std::move(field));
        PageGrid foot = SettingsPageBuild::makeNested(spec.prefix + QStringLiteral("_footer"), 3, 0,
                                                      1, 2, 8);
        foot.columnTracks = {PageTrackSize::starWeight(1.4), PageTrackSize::starWeight()};
        PageCell save = SettingsPageBuild::cell(spec.prefix + QStringLiteral("_save"),
                                                QStringLiteral("Save"), 0, 0,
                                                QStringLiteral("compose.saveName"), accent);
        paintSave(save, accent);
        foot.cells.push_back(std::move(save));
        PageCell cancel = SettingsPageBuild::cell(spec.prefix + QStringLiteral("_cancel"),
                                                  QStringLiteral("Cancel"), 0, 1,
                                                  QStringLiteral("compose.cancelName"), key);
        paintSurface(cancel, key);
        foot.cells.push_back(std::move(cancel));
        grid.subGrids.push_back(std::move(foot));
    } else {
        grid.subGrids.push_back(searchBar(
            spec.searchId, spec.query, spec.placeholder, 2, key, accent, spec.clearCommand, true,
            spec.addCommand.isEmpty() ? QString() : QStringLiteral("Add"), spec.addCommand,
            SearchFill::Accent));
    }
    return doc;
}

} // namespace gazer::compose_detail
