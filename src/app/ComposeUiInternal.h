#pragma once

#include "app/SettingsPageBuild.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QtMath>
#include <QVector>

namespace gazer::compose_detail {

constexpr int kOverlayListRows = 10;
constexpr int kOverlayScrollTrack = 6;
constexpr int kOverlayBodyCols = 20;
constexpr int kOverlayListColSpan = 19;

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
    return body;
}

inline PageGrid overlayList(int columns)
{
    PageGrid list;
    list.id = QStringLiteral("list");
    list.nested = true;
    list.row = 0;
    list.col = 0;
    list.colSpan = kOverlayListColSpan;
    list.rows = kOverlayListRows;
    list.columns = columns;
    list.gapPx = 8;
    return list;
}

/// Narrow strip: square-ish ArrowUp / ArrowDown and a 6-slot track.
inline PageGrid overlayScrollbar(const QString& prevCmd, const QString& nextCmd,
                                 const QString& gotoPrefix, int offset, int maxOffset,
                                 const QColor& surface, const QColor& accent, const QColor& well)
{
    using SettingsPageBuild::cell;
    using SettingsPageBuild::scrollHit;
    PageGrid scroll;
    scroll.id = QStringLiteral("scroll");
    scroll.nested = true;
    scroll.row = 0;
    scroll.col = kOverlayListColSpan;
    scroll.colSpan = 1;
    scroll.rows = 3;
    scroll.columns = 1;
    scroll.rowWeights = {1.0, 6.0, 1.0};
    scroll.gapPx = 4;
    scroll.marginPx = 4;
    scroll.style.background = surface;
    scroll.style.thickness = PageBox::all(0);
    scroll.style.radius = PageBox::all(16);
    const bool canPrev = maxOffset > 0 && offset > 0;
    const bool canNext = maxOffset > 0 && offset < maxOffset;
    scroll.cells.push_back(cell(QStringLiteral("sb_up"), {}, 0, 0, canPrev ? prevCmd : QString(),
                                surface, 1, canPrev ? QString() : QStringLiteral("label"), {},
                                QStringLiteral("ArrowPointingToTop")));
    scroll.cells.push_back(cell(QStringLiteral("sb_down"), {}, 2, 0, canNext ? nextCmd : QString(),
                                surface, 1, canNext ? QString() : QStringLiteral("label"), {},
                                QStringLiteral("ArrowPointingToBottom")));

    PageGrid track;
    track.id = QStringLiteral("track");
    track.nested = true;
    track.row = 1;
    track.col = 0;
    track.rows = kOverlayScrollTrack;
    track.columns = 1;
    track.gapPx = 2;
    track.marginPx = 4;
    track.style.background = well;
    track.style.thickness = PageBox::all(0);
    track.style.radius = PageBox::all(10);
    const QColor clear(0, 0, 0, 0);
    if (maxOffset > 0) {
        const int last = kOverlayScrollTrack - 1;
        const int thumbStart = qBound(
            0, int(qRound(double(offset) * double(last) / double(maxOffset))), last);
        for (int i = 0; i < kOverlayScrollTrack; ++i) {
            if (i == thumbStart) {
                track.cells.push_back(scrollHit(QStringLiteral("thumb"), i, 1,
                                                gotoPrefix + QString::number(offset), accent, 8));
                continue;
            }
            const int pos =
                qBound(0, int(qRound(double(i) * double(maxOffset) / double(last))), maxOffset);
            track.cells.push_back(scrollHit(QStringLiteral("sb_%1").arg(i), i, 1,
                                            gotoPrefix + QString::number(pos), clear, 0));
        }
    }
    scroll.subGrids.push_back(std::move(track));
    return scroll;
}

inline QColor parseColor(const QString& hex, const QColor& fallback)
{
    const QColor c(hex);
    return c.isValid() ? c : fallback;
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
            const QColor bg = it.on ? accent : parseColor(it.color, surface);
            cell.style.background = bg;
            cell.style.foreground = ThemeColors::contrastOn(bg);
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

inline const QStringList& paletteColors()
{
    static const QStringList k = {
        QStringLiteral("#a0a0a0"), QStringLiteral("#cc0000"), QStringLiteral("#e69138"),
        QStringLiteral("#f1c232"), QStringLiteral("#6aa84f"), QStringLiteral("#45818e"),
        QStringLiteral("#3c78d8"), QStringLiteral("#3d85c6"), QStringLiteral("#674ea7"),
        QStringLiteral("#a64d79"), QStringLiteral("#808080"), QStringLiteral("#990000"),
        QStringLiteral("#b45f06"), QStringLiteral("#bf9000"), QStringLiteral("#38761d"),
        QStringLiteral("#134f5c"), QStringLiteral("#1155cc"), QStringLiteral("#0b5394"),
        QStringLiteral("#351c75"), QStringLiteral("#741b47"), QStringLiteral("#606060"),
        QStringLiteral("#660000"), QStringLiteral("#783f04"), QStringLiteral("#7f6000"),
        QStringLiteral("#274e13"), QStringLiteral("#0c343d"), QStringLiteral("#1c4587"),
        QStringLiteral("#073763"), QStringLiteral("#20124d"), QStringLiteral("#4c1130"),
        QStringLiteral("#000000"), QStringLiteral("#ffffff"), QStringLiteral("#dddddd"),
        QStringLiteral("#bbbbbb"), QStringLiteral("#999999"), QStringLiteral("#777777"),
        QStringLiteral("#555555"), QStringLiteral("#333333"), QStringLiteral("#111111"),
    };
    return k;
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

} // namespace gazer::compose_detail
