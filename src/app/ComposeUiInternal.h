#pragma once

#include "app/SettingsPageBuild.h"
#include "layout/PageDim.h"
#include "layout/PageTypes.h"
#include "ui/ScrollBar.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace gazer::compose_detail {

constexpr int kOverlayListRows = 10;
/// Rows on a search sheet that sits above the Speak keyboard.
constexpr int kSearchListRows = 5;
/// Rail chooser. Tag and style sheets use `kNameCols`.
constexpr int kTileCols = 4;
constexpr int kTileRows = 3;
constexpr int kTilePage = kTileCols * kTileRows;
/// Tag and style sheets: three names on each row. The row count fills the sheet.
constexpr int kNameCols = 3;
/// Gap between those name rows. The fit math uses the same value.
constexpr int kNameListGap = 8;

/// One Speak keyboard row, from the authored keys band. Fallback matches `compose.xml`.
inline QString keyboardRowExpr(const PageDocument& authored)
{
    const QString fallback = QStringLiteral("((A_ScreenHeight-40)*4/12-16)/3");
    if (authored.grids.isEmpty()) {
        return fallback;
    }
    const PageGrid& board = authored.grids[0];
    const PageGrid* keys = authored.findGrid(QStringLiteral("keys"));
    if (!keys || keys->row < 0) {
        return fallback;
    }
    const int boardRows = qMax(1, board.rows);
    if (keys->row >= boardRows) {
        return fallback;
    }
    double parts = 0.0;
    double keyParts = 0.0;
    for (int i = 0; i < boardRows; ++i) {
        const PageTrackSize track = i < board.rowTracks.size() ? board.rowTracks.at(i)
                                                                : PageTrackSize::starWeight();
        if (!track.isStar()) {
            return fallback;
        }
        const double weight = track.star > 0.0 ? track.star : 1.0;
        parts += weight;
        if (i == keys->row) {
            keyParts = weight;
        }
    }
    if (parts <= 0.0 || keyParts <= 0.0) {
        return fallback;
    }
    const int boardTrim = 2 * qMax(0, board.marginPx) + qMax(0, board.gapPx) * (boardRows - 1);
    const int keyRows = qMax(1, keys->rows);
    const int keyChrome =
        2 * qMax(0, keys->marginPx) + qMax(0, keys->gapPx) * (keyRows - 1);
    auto num = [](double v) {
        const qint64 whole = qRound64(v);
        if (qAbs(v - double(whole)) < 1e-6) {
            return QString::number(whole);
        }
        return QString::number(v, 'g', 12);
    };
    return QStringLiteral("((A_ScreenHeight-%1)*%2/%3-%4)/%5")
        .arg(boardTrim)
        .arg(num(keyParts))
        .arg(num(parts))
        .arg(keyChrome)
        .arg(keyRows);
}

inline PageTrackSize keyboardRowTrack(const PageDocument& authored)
{
    return PageTrackSize::fromDim(PageDim::expression(keyboardRowExpr(authored)));
}

inline int namePageSize(int rows)
{
    return qMax(1, rows) * kNameCols;
}

/// First index that keeps `index` on screen. `rows` is the visible name-row count.
inline int namePageFor(int index, int total, int rows)
{
    const int visible = namePageSize(rows);
    const int rowStart = (qMax(0, index) / kNameCols) * kNameCols;
    const int page = qMax(0, rowStart - (qMax(1, rows) - 1) * kNameCols);
    return qBound(0, page, qMax(0, total - visible));
}
/// History rows above the keyboard. Each record is one row.
constexpr int kHistoryCards = 5;
constexpr int kOverlayBodyCols = 20;
constexpr int kOverlayListColSpan = 19;
/// Scroll column width and the up/down arrow buttons, in pixels.
constexpr int kScrollChromePx = 72;
constexpr auto kOverlayScrollTrackId = QLatin1String("sb_track");

inline PageAction commandAction(const QString& name)
{
    PageAction a;
    a.type = PageActionType::Command;
    a.command = name;
    return a;
}

inline PageGrid overlayBody(int row, int colSpan = 8)
{
    PageGrid body;
    body.id = QStringLiteral("body");
    body.nested = true;
    body.row = row;
    body.col = 0;
    body.colSpan = colSpan;
    body.rows = 1;
    body.columns = kOverlayBodyCols;
    body.gapPx = 8;
    body.columnTracks.reserve(kOverlayBodyCols);
    for (int i = 0; i < kOverlayListColSpan; ++i) {
        body.columnTracks.push_back(PageTrackSize::starWeight());
    }
    body.columnTracks.push_back(PageTrackSize::fromDim(PageDim::pixels(kScrollChromePx)));
    return body;
}

inline PageGrid overlayList(int columns, int rows = kOverlayListRows)
{
    PageGrid list;
    list.id = QStringLiteral("list");
    list.nested = true;
    list.row = 0;
    list.col = 0;
    list.colSpan = kOverlayListColSpan;
    list.rows = rows;
    list.columns = columns;
    list.gapPx = 8;
    return list;
}

/// Gaze scrollbar with an up button, the track, and a down button.
/// The track lock-on matches a volume slider (`kTrackLockMs`).
inline PageGrid overlayScrollbar(int offset, int visible, int total, const QColor& surface,
                                 const QString& upCommand, const QString& downCommand)
{
    using SettingsPageBuild::cell;
    const int maxOff = qMax(0, total - qMax(0, visible));
    PageGrid scroll;
    scroll.id = QStringLiteral("scroll");
    scroll.nested = true;
    scroll.row = 0;
    scroll.col = kOverlayListColSpan;
    scroll.colSpan = 1;
    scroll.rows = 3;
    scroll.columns = 1;
    scroll.gapPx = 6;
    scroll.marginPx = 4;
    scroll.rowTracks = {PageTrackSize::fromDim(PageDim::pixels(kScrollChromePx)),
                        PageTrackSize::starWeight(),
                        PageTrackSize::fromDim(PageDim::pixels(kScrollChromePx))};
    scroll.style.background = surface;
    scroll.style.thickness = PageBox::all(0);
    scroll.style.radius = PageBox::all(16);

    auto endButton = [&](const QString& id, int row, bool on, const QString& command,
                         const QString& icon) {
        PageCell button = cell(id, {}, row, 0, on ? command : QString(), surface, 1,
                               on ? QString() : QStringLiteral("label"), {}, icon);
        if (!on) {
            button.style.foreground =
                ThemeColors::mix(surface, ThemeColors::contrastOn(surface), 0.4);
        }
        return button;
    };
    scroll.cells.push_back(
        endButton(QStringLiteral("sb_up"), 0, offset > 0, upCommand, QStringLiteral("ArrowUp")));
    PageCell track = cell(QString(kOverlayScrollTrackId), {}, 1, 0, {}, QColor(), 1,
                          QStringLiteral("scrollbar"));
    track.caption = ScrollBar::formatSpec(offset, visible, total);
    track.dwell.scanGrace = kTrackLockMs;
    track.style.thickness = PageBox::all(0);
    track.style.radius = PageBox::all(10);
    scroll.cells.push_back(std::move(track));
    scroll.cells.push_back(endButton(QStringLiteral("sb_down"), 2, offset < maxOff, downCommand,
                                    QStringLiteral("ArrowDown")));
    return scroll;
}

inline QColor parseColor(const QString& hex, const QColor& fallback)
{
    const QColor c(hex);
    return c.isValid() ? c : fallback;
}

/// Fill from `background`. A custom `border` is also the label color.
inline void paintItemFace(PageCell& cell, const QString& background, const QString& border,
                          const QColor& surface)
{
    const QString bgHex = background.trimmed();
    const QString borderHex = border.trimmed();
    const QColor fill = bgHex.isEmpty() ? surface : parseColor(bgHex, surface);
    cell.style.background = fill;
    cell.style.radius = PageBox::all(12);
    if (!borderHex.isEmpty()) {
        const QColor ink = parseColor(borderHex, fill);
        cell.style.borderColor = ink;
        cell.style.thickness = PageBox::all(2);
        cell.style.foreground = ink;
        return;
    }
    cell.style.borderColor = QColor();
    cell.style.thickness = PageBox::all(0);
    cell.style.foreground = ThemeColors::contrastOn(fill);
}

/// + beside `face`. No fill. Border is the speak border when set, otherwise the speak fill.
/// No custom speak fill: the glyph matches the speak label.
inline void paintInsertChip(PageCell& plus, const PageCell& face, const QString& background,
                            const QString& border)
{
    const QColor faceFill = face.style.background.parsed();
    const QString borderHex = border.trimmed();
    const QColor edge = borderHex.isEmpty() ? faceFill : parseColor(borderHex, faceFill);
    plus.style = {};
    plus.style.background = QColor(0, 0, 0, 0);
    plus.style.borderColor = edge;
    plus.style.thickness = PageBox::all(3);
    plus.style.foreground = background.trimmed().isEmpty() ? face.style.foreground : PageColor(edge);
    plus.style.radius = PageBox::of(0, 10, 10, 0);
}

inline void stampEditableGrid(PageGrid* grid, bool editing, const QColor& accent)
{
    if (!grid) {
        return;
    }
    grid->style.radius = PageBox::all(16);
    if (editing) {
        grid->style.borderColor = accent;
        grid->style.thickness = PageBox::all(3);
    } else {
        grid->style.borderColor = QColor();
        grid->style.thickness = PageBox::all(0);
    }
}

struct ChoiceSlot {
    QString label;
    QString caption;
    QString icon;
    QString color;
    QString border;
    QString useCommand;
    QString editCommand;
    QString activeState;
    bool selected = false;
    bool on = false;
};

inline void fillChoiceGrid(PageGrid* grid, int rows, int cols, const QString& idPrefix,
                           const QVector<ChoiceSlot>& items, bool editing,
                           const QString& emptyCommand, const QColor& surface, const QColor& accent,
                           const QColor& value)
{
    if (!grid) {
        return;
    }
    stampEditableGrid(grid, editing, accent);
    grid->cells.clear();
    grid->rows = rows;
    grid->columns = cols;
    const int cap = rows * cols;
    for (int i = 0; i < cap; ++i) {
        PageCell cell;
        cell.id = QStringLiteral("%1%2").arg(idPrefix).arg(i);
        cell.row = i / cols;
        cell.col = i % cols;
        if (i < items.size()) {
            const ChoiceSlot& it = items.at(i);
            cell.label = it.label;
            cell.caption = it.caption;
            cell.icon = it.icon;
            cell.role = QStringLiteral("choice");
            cell.activeState = it.selected ? QStringLiteral("compose.editMode") : it.activeState;
            if (it.on) {
                cell.style.background = accent;
                cell.style.foreground = ThemeColors::contrastOn(accent);
            } else {
                paintItemFace(cell, it.color, it.border, surface);
            }
            cell.actions.push_back(commandAction(editing ? it.editCommand : it.useCommand));
        } else {
            cell.style.background = value;
            if (editing && !emptyCommand.isEmpty()) {
                cell.icon = QStringLiteral("add");
                cell.actions.push_back(commandAction(emptyCommand));
            } else {
                cell.role = QStringLiteral("label");
            }
        }
        grid->cells.push_back(cell);
    }
}

inline const QStringList& paletteIcons()
{
    static const QStringList k = {
        QStringLiteral("chat"),
        QStringLiteral("home"),
        QStringLiteral("favorite"),
        QStringLiteral("restaurant"),
        QStringLiteral("waterDrop"),
        QStringLiteral("wc"),
        QStringLiteral("hotel"),
        QStringLiteral("medicalServices"),
        QStringLiteral("sentimentSatisfied"),
        QStringLiteral("sentimentSad"),
        QStringLiteral("help"),
        QStringLiteral("person"),
        QStringLiteral("groups"),
        QStringLiteral("school"),
        QStringLiteral("work"),
        QStringLiteral("musicNote"),
        QStringLiteral("pets"),
        QStringLiteral("phoneInTalk"),
        QStringLiteral("localCafe"),
        QStringLiteral("directionsCar"),
        QStringLiteral("childCare"),
        QStringLiteral("book"),
        QStringLiteral("sunny"),
        QStringLiteral("sports"),
    };
    return k;
}

inline constexpr int kQueryLimit = 40;

using QueryCharOk = bool (*)(QChar);

inline bool iconQueryChar(QChar c)
{
    return c.isLetterOrNumber() || c == u' ' || c == u'_';
}

/// One character from a Send action. `ok` rejects characters the field cannot keep.
inline QString queryInsertion(const PageAction& action, QueryCharOk ok)
{
    const QString key = action.sendKey.trimmed();
    QString add;
    if (key.size() == 1) {
        add = key;
    } else if (key.compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
        add = QStringLiteral(" ");
    } else {
        return {};
    }
    if (ok && !ok(add.at(0))) {
        return {};
    }
    return add;
}

inline bool appendQuery(QString& text, const QString& add, int limit = kQueryLimit)
{
    if (add.isEmpty() || text.size() >= limit) {
        return false;
    }
    text += add.left(limit - text.size());
    return true;
}

enum class QueryCommand { Ignored, Edited, Enter };

/// Backspace, space, and Enter for a captured search. Other commands are ignored
/// here; the caller still swallows them while the capture is active.
/// Height of the Speak area above the keyboard, so a sheet leaves the keys free.
inline QString aboveKeyboardHeight(const PageDocument& authored)
{
    const QString fallback = QStringLiteral("(A_ScreenHeight-40)*8/12+24");
    if (authored.grids.isEmpty()) {
        return fallback;
    }
    const PageGrid* keys = authored.findGrid(QStringLiteral("keys"));
    if (!keys) {
        return fallback;
    }
    const std::optional<PageDimParse::StarBand> band =
        PageDimParse::starBandBeforeRow(authored.grids[0], keys->row);
    if (!band || band->topExpr.isEmpty()) {
        return fallback;
    }
    return band->topExpr;
}

/// Item-editor sheet: margin 12, title 72, footer 72, two board gaps of 8,
/// and three gaps of 16 between the four field rows. No extra group margin.
inline constexpr int kFormFieldRowGap = 16;
inline constexpr int kFormFieldRowTop = 92;
inline constexpr int kFormFieldChrome = 220;

/// One of the four field rows, so a text popup matches that row.
inline QString formFieldRowHeight(const PageDocument& authored)
{
    return QStringLiteral("((%1)-%2)/4")
        .arg(aboveKeyboardHeight(authored))
        .arg(kFormFieldChrome);
}

/// One editor column. Board margin 12 on each side and a 12px gap between columns.
inline QString formColumnWidth(const PageDim& board)
{
    return QStringLiteral("((%1)-36)/2").arg(PageDimParse::token(board));
}

/// Left column starts at the board margin. The right column starts after that width and the gap.
inline PageDim formColumnOrigin(bool right, const PageDim& board)
{
    if (!right) {
        return PageDim::pixels(12);
    }
    return PageDim::expression(QStringLiteral("24+((%1)-36)/2").arg(PageDimParse::token(board)));
}

/// Settings page chrome: group cards, joined controls, and radius-10 buttons.
inline void addSettingsChrome(PageDocument& doc)
{
    auto put = [&](const char* id, const char* bg, const PageBox& radius) {
        PageChrome chrome;
        chrome.background.token = QLatin1String(bg);
        chrome.thickness = PageBox::all(0.0);
        chrome.radius = radius;
        doc.styles.insert(QLatin1String(id), chrome);
    };
    put("group", "bg95", PageBox::all(16.0));
    put("row", "transparent", PageBox::all(0.0));
    put("plain", "transparent", PageBox::all(0.0));
    put("join", "transparent", PageBox::all(0.0));
    put("joinLeft", "transparent", PageBox::of(10.0, 0.0, 0.0, 10.0));
    put("joinRight", "transparent", PageBox::of(0.0, 10.0, 10.0, 0.0));
    put("button", "transparent", PageBox::all(10.0));
}

/// Height of the Speak file browser. Settings import/export sizes the borrowed
/// keyboard as `A_ScreenHeight` minus this string, so the two edges meet.
/// Other Speak sheets still use `aboveKeyboardHeight`.
inline QString filesOverlayHeight()
{
    return QStringLiteral("(A_ScreenHeight-56)*2/3+24");
}

/// Search line that already owns the caret. Typing goes into `query`.
inline PageCell searchField(const QString& id, const QString& query, const QString& placeholder,
                            int row, int col, int colSpan, const QColor& surface,
                            const QColor& accent)
{
    PageCell field = SettingsPageBuild::cell(id, query.isEmpty() ? placeholder : query, row, col,
                                             {}, surface, colSpan, QStringLiteral("display"), {},
                                             QStringLiteral("search"));
    field.caretIndex = query.size();
    field.style.borderColor = accent;
    field.style.thickness = PageBox::all(2);
    field.style.radius = PageBox::all(12);
    return field;
}

inline void paintOutline(PageCell& cell, const QColor& ink)
{
    cell.style.background = QColor(0, 0, 0, 0);
    cell.style.foreground = ink;
    cell.style.borderColor = ink;
    cell.style.thickness = PageBox::all(2);
}

inline QColor accentOf(const ThemeColors& theme)
{
    return theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
}

inline QColor dangerOf(const ThemeColors& theme)
{
    return theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
}

inline QColor fieldOf(const ThemeColors& theme)
{
    return theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
}

/// Save and Done are the accent fill. Cancel and Close are the surface fill.
/// Delete is a danger outline. The same words use the same fill on every sheet.
inline void paintSave(PageCell& cell, const QColor& accent)
{
    cell.style.background = accent;
    cell.style.foreground = ThemeColors::contrastOn(accent);
    cell.style.borderColor = QColor();
    cell.style.thickness = PageBox::all(0);
    cell.style.radius = PageBox::all(12);
}

inline void paintSurface(PageCell& cell, const QColor& surface)
{
    cell.style.background = surface;
    cell.style.foreground = ThemeColors::contrastOn(surface);
    cell.style.borderColor = QColor();
    cell.style.thickness = PageBox::all(0);
    cell.style.radius = PageBox::all(12);
}

/// Text on a parent bar. No fill and no stroke, so the bar stays one piece.
inline void paintFlat(PageCell& cell)
{
    cell.style.background = QColor(0, 0, 0, 0);
    cell.style.foreground = QColor();
    cell.style.borderColor = QColor();
    cell.style.thickness = PageBox::all(0);
    cell.style.radius = PageBox::all(0);
}

inline void paintDanger(PageCell& cell, const QColor& danger)
{
    paintOutline(cell, danger);
    cell.style.radius = PageBox::all(12);
}

inline void paintSelected(PageCell& cell, const QColor& accent, const QColor& surface)
{
    cell.style.background = surface;
    cell.style.foreground = ThemeColors::contrastOn(surface);
    cell.style.borderColor = accent;
    cell.style.thickness = PageBox::all(2);
    cell.style.radius = PageBox::all(12);
}

inline PageTrackSize trackPx(int px)
{
    return PageTrackSize::fromDim(PageDim::pixels(px));
}

/// One style or tag. Star on the left. Delete on the right only when it is not
/// a favorite. The right column stays so the names line up.
inline PageGrid nameCard(const QString& id, int row, int col, const QString& label,
                         const QString& applyCommand, const QString& favCommand, bool favorite,
                         const QString& trashCommand, bool selected, const QColor& value,
                         const QColor& accent, const QColor& key, const QColor& danger)
{
    PageGrid tile = SettingsPageBuild::makeNested(id, row, col, 1, 3, 4);
    tile.columnTracks = {trackPx(64), PageTrackSize::starWeight(), trackPx(64)};
    tile.marginPx = 4;
    tile.style.radius = PageBox::all(12);
    const QColor rim = ThemeColors::mix(value, ThemeColors::contrastOn(value), 0.28);
    tile.style.background = value;
    tile.style.borderColor = selected ? accent : rim;
    tile.style.thickness = PageBox::all(selected ? 2 : 1);
    PageCell star = SettingsPageBuild::cell(id + QStringLiteral("_fav"), {}, 0, 0, favCommand, key,
                                            1, {}, {}, QStringLiteral("star"));
    star.activeState = favCommand;
    paintSurface(star, key);
    tile.cells.push_back(std::move(star));
    PageCell name = SettingsPageBuild::cell(id + QStringLiteral("_name"), label, 0, 1,
                                            applyCommand, QColor());
    name.textStyle = QStringLiteral("row");
    paintFlat(name);
    tile.cells.push_back(std::move(name));
    if (!favorite) {
        PageCell trash = SettingsPageBuild::cell(id + QStringLiteral("_trash"), {}, 0, 2,
                                                 trashCommand, QColor(), 1, {}, {},
                                                 QStringLiteral("delete"));
        paintDanger(trash, danger);
        tile.cells.push_back(std::move(trash));
    }
    return tile;
}

/// Title row. Cancel abandons an edit and says Cancel. Otherwise a round Close X,
/// unless `showClose` is false.
inline PageGrid speakTitleBar(const QString& heading, bool cancel, const QColor& surface,
                              bool showClose = true)
{
    const bool hasButton = cancel || showClose;
    PageGrid title = SettingsPageBuild::makeNested(QStringLiteral("titlebar"), 0, 0, 1,
                                                   hasButton ? 2 : 1, 8);
    if (hasButton) {
        title.columnTracks = {PageTrackSize::starWeight(), trackPx(cancel ? 160 : 72)};
    }
    PageCell titleCell = SettingsPageBuild::cell(QStringLiteral("title"), heading, 0, 0, {},
                                                 QColor(), 1, QStringLiteral("label"));
    titleCell.textStyle = QStringLiteral("title");
    title.cells.push_back(std::move(titleCell));
    if (!hasButton) {
        return title;
    }
    if (cancel) {
        PageCell button = SettingsPageBuild::cell(QStringLiteral("close"), QStringLiteral("Cancel"),
                                                  0, 1, QStringLiteral("compose.cancelName"),
                                                  surface);
        paintSurface(button, surface);
        title.cells.push_back(std::move(button));
    } else {
        PageCell button = SettingsPageBuild::cell(QStringLiteral("close"), {}, 0, 1, {}, surface, 1,
                                                  {}, {}, QStringLiteral("close"));
        paintSurface(button, surface);
        button.style.radius = PageBox::all(1000);
        button.actions.push_back(SettingsPageBuild::closeSelf());
        title.cells.push_back(std::move(button));
    }
    return title;
}

/// Fill for the optional button between the search field and Done.
enum class SearchFill { Surface, Danger, Accent };

/// Search line above the keyboard. The clear mark sits inside the field.
/// `done` is false while naming, where Save and Cancel already sit under the field.
/// `middle` is an optional button between the field and Done.
inline PageGrid searchBar(const QString& id, const QString& query, const QString& placeholder,
                          int row, const QColor& surface, const QColor& accent,
                          const QString& clearCommand, bool done = true,
                          const QString& middleLabel = {}, const QString& middleCommand = {},
                          SearchFill middleFill = SearchFill::Surface, const QColor& middleInk = {})
{
    const bool middle = !middleLabel.isEmpty();
    int columns = 1;
    if (middle) {
        ++columns;
    }
    if (done) {
        ++columns;
    }
    PageGrid bar = SettingsPageBuild::makeNested(id + QStringLiteral("_bar"), row, 0, 1, columns, 8);
    bar.columnTracks.push_back(PageTrackSize::starWeight());
    if (middle) {
        bar.columnTracks.push_back(trackPx(middleFill == SearchFill::Accent ? 140 : 200));
    }
    if (done) {
        bar.columnTracks.push_back(trackPx(140));
    }

    PageGrid field = SettingsPageBuild::makeNested(id + QStringLiteral("_cap"), 0, 0, 1, 2, 0);
    field.columnTracks = {PageTrackSize::starWeight(), trackPx(48)};
    field.marginPx = 4;
    field.style.background = surface;
    field.style.borderColor = accent;
    field.style.thickness = PageBox::all(2);
    field.style.radius = PageBox::all(12);
    PageCell text = searchField(id, query, placeholder, 0, 0, 1, surface, accent);
    text.style.background = QColor(0, 0, 0, 0);
    text.style.borderColor = QColor();
    text.style.thickness = PageBox::all(0);
    text.style.radius = PageBox::all(0);
    field.cells.push_back(std::move(text));
    PageCell clear = SettingsPageBuild::cell(id + QStringLiteral("_clear"), {}, 0, 1, clearCommand,
                                             QColor(), 1, {}, {}, QStringLiteral("close"));
    paintFlat(clear);
    clear.style.foreground = ThemeColors::contrastOn(surface);
    field.cells.push_back(std::move(clear));
    bar.subGrids.push_back(std::move(field));

    int col = 1;
    if (middle) {
        PageCell extra = SettingsPageBuild::cell(id + QStringLiteral("_extra"), middleLabel, 0, col,
                                                 middleCommand, QColor());
        if (middleFill == SearchFill::Accent) {
            paintSave(extra, accent);
        } else if (middleFill == SearchFill::Danger) {
            paintDanger(extra, middleInk.isValid() ? middleInk : accent);
        } else {
            paintSurface(extra, surface);
        }
        bar.cells.push_back(std::move(extra));
        ++col;
    }
    if (done) {
        PageCell button = SettingsPageBuild::cell(id + QStringLiteral("_done"),
                                                  QStringLiteral("Done"), 0, col, {}, accent);
        paintSave(button, accent);
        button.actions.push_back(SettingsPageBuild::closeSelf());
        bar.cells.push_back(std::move(button));
    }
    return bar;
}

struct RailActionSpec {
    QString command;
    QString label;
    QString icon;
};

inline QVector<RailActionSpec> railActionCatalog()
{
    return {
        {QStringLiteral("compose.openHistory"), QStringLiteral("History"), QStringLiteral("history")},
        {QStringLiteral("compose.openVoices"), QStringLiteral("Voice"),
         QStringLiteral("recordVoiceOver")},
        {QStringLiteral("compose.openStyles"), QStringLiteral("Style"), QStringLiteral("palette")},
        {QStringLiteral("compose.openTags"), QStringLiteral("Tag"), QStringLiteral("sell")},
        {QStringLiteral("compose.pin"), QStringLiteral("Edit"), QStringLiteral("edit")},
        {QStringLiteral("compose.clear"), QStringLiteral("Clear"), QStringLiteral("keyBackspace")},
        {QStringLiteral("compose.speak"), QStringLiteral("Speak"), QStringLiteral("Speak")},
        {QStringLiteral("compose.undo"), QStringLiteral("Undo"), QStringLiteral("undo")},
        {QStringLiteral("compose.redo"), QStringLiteral("Redo"), QStringLiteral("redo")},
        {QStringLiteral("compose.openFiles"), QStringLiteral("Files"), QStringLiteral("folder")},
        {QStringLiteral("compose.importFile"), QStringLiteral("Import"), QStringLiteral("folder")},
        {QStringLiteral("compose.exportFile"), QStringLiteral("Export"),
         QStringLiteral("description")},
        {QStringLiteral("compose.selectAll"), QStringLiteral("Select all"), QStringLiteral("check")},
        {QStringLiteral("compose.deselect"), QStringLiteral("Deselect"), QStringLiteral("close")},
        {QStringLiteral("compose.cut"), QStringLiteral("Cut"), QStringLiteral("contentCut")},
        {QStringLiteral("compose.copy"), QStringLiteral("Copy"), QStringLiteral("contentCopy")},
        {QStringLiteral("compose.paste"), QStringLiteral("Paste"), QStringLiteral("contentPaste")},
    };
}

inline RailActionSpec railActionLookup(const QString& command)
{
    for (const RailActionSpec& spec : railActionCatalog()) {
        if (spec.command == command) {
            return spec;
        }
    }
    RailActionSpec spec;
    spec.command = command;
    spec.label = command.section(QLatin1Char('.'), -1);
    if (spec.label.isEmpty()) {
        spec.label = command;
    }
    return spec;
}

inline QStringList defaultRailCommands()
{
    return {QStringLiteral("compose.openHistory"), QStringLiteral("compose.openVoices"),
            QStringLiteral("compose.openStyles"), QStringLiteral("compose.openTags"),
            QStringLiteral("compose.pin")};
}

inline QueryCommand editQuery(QString& text, const QString& command, QueryCharOk ok,
                              int limit = kQueryLimit)
{
    const QString c = command.trimmed();
    if (c == QLatin1String("backspace") || c == QLatin1String("compose.backspace")) {
        if (text.isEmpty()) {
            return QueryCommand::Ignored;
        }
        text.chop(1);
        return QueryCommand::Edited;
    }
    if (c == QLatin1String("space")) {
        if (ok && !ok(QLatin1Char(' '))) {
            return QueryCommand::Ignored;
        }
        return appendQuery(text, QStringLiteral(" "), limit) ? QueryCommand::Edited
                                                             : QueryCommand::Ignored;
    }
    if (c == QLatin1String("enter")) {
        return QueryCommand::Enter;
    }
    return QueryCommand::Ignored;
}

} // namespace gazer::compose_detail
