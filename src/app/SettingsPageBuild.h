#pragma once

#include "layout/PageDim.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>
#include <QtGlobal>
#include <QVector>

namespace gazer {
namespace SettingsPageBuild {

inline PageAction command(const QString& name)
{
    PageAction a;
    a.type = PageActionType::Command;
    a.command = name;
    return a;
}

inline PageAction closeSelf()
{
    PageAction a;
    a.type = PageActionType::Nav;
    a.verb = PageVerb::Close;
    a.targetScope = PageNavScope::Self;
    return a;
}

inline PageCell cell(const QString& id, const QString& label, int row, int col,
                     const QString& commandName, const QColor& bg, int colSpan = 1,
                     const QString& role = {}, const QString& caption = {},
                     const QString& icon = {})
{
    PageCell c;
    c.id = id;
    c.label = label;
    c.caption = caption;
    c.icon = icon;
    c.role = role;
    c.row = row;
    c.col = col;
    c.colSpan = colSpan;
    if (bg.isValid()) {
        c.style.background = bg;
        c.style.foreground = ThemeColors::contrastOn(bg);
    }
    if (c.isInteractive() && !commandName.isEmpty()) {
        c.actions.push_back(command(commandName));
    }
    return c;
}

inline PageCell scrollHit(const QString& id, int row, int rowSpan, const QString& commandName,
                          const QColor& bg, double radius)
{
    PageCell c = cell(id, {}, row, 0, commandName, bg, 1,
                      commandName.isEmpty() ? QStringLiteral("label") : QString());
    c.rowSpan = qMax(1, rowSpan);
    c.style.thickness = PageBox::all(0);
    c.style.radius = PageBox::all(radius);
    c.style.borderColor = QColor(0, 0, 0, 0);
    return c;
}

inline PageGrid makeNested(const QString& id, int row, int col, int rows, int cols, int gap)
{
    PageGrid g;
    g.id = id;
    g.nested = true;
    g.row = row;
    g.col = col;
    g.rows = rows;
    g.columns = cols;
    g.gapPx = gap;
    return g;
}

inline void adoptCallerBoard(PageGrid& g, const PageDocument& caller)
{
    if (caller.grids.isEmpty() || !caller.grids[0].size.isSet()) {
        g.desktopMode = true;
        g.anchor = PageAnchor::Top;
        g.offset.x = PageDim::pixels(0);
        g.offset.y = PageDim::pixels(0);
        g.size.x = PageDim::expression(
            QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
        g.size.y = PageDim::expression(QStringLiteral("A_ScreenHeight"));
        g.gapPx = 6;
        g.marginPx = 12;
        return;
    }
    const PageGrid& src = caller.grids[0];
    g.desktopMode = src.desktopMode;
    g.anchor = src.anchor;
    g.offset = src.offset;
    g.size = src.size;
    g.gapPx = src.gapPx;
    g.marginPx = src.marginPx;
}

inline void initGrid(PageDocument& doc, int cols, int rows, int widthPx, int heightPx, int gap,
                     int margin, const ThemeColors& theme)
{
    PageGrid g;
    g.id = QStringLiteral("board");
    g.desktopMode = true;
    g.anchor = PageAnchor::Center;
    g.size.x = PageDim::pixels(widthPx);
    g.size.y = PageDim::pixels(heightPx);
    g.rows = rows;
    g.columns = cols;
    g.gapPx = gap;
    g.marginPx = margin;
    QColor bg = theme.bgMain.isValid() ? theme.bgMain : QColor(10, 10, 11);
    bg.setAlpha(255);
    g.style.background = bg;
    if (theme.border.isValid()) {
        g.style.borderColor = theme.border;
    }
    g.style.radius = PageBox::all(PageChrome::kDefaultRadius);
    g.style.thickness = PageBox::all(PageChrome::kDefaultThickness);
    doc.grids.push_back(std::move(g));
}

/// Fallback Speak width when the live compose grid is not available.
inline PageDim defaultSpeakBoardWidth()
{
    return PageDim::expression(QStringLiteral("1.5*A_ScreenHeight"));
}

/// Same width as Speak. Default height covers title + topics + soundboard
/// (`rowWeights` 1,1,3,3,4 → 5/12), leaving the phrase and keyboard free.
inline void initTopOverlay(PageDocument& doc, int cols, int rows, const QVector<double>& weights,
                           int gap, int margin, const ThemeColors& theme,
                           const QString& heightExpr = QStringLiteral("A_ScreenHeight/12*5+8"),
                           PageDim width = {})
{
    PageGrid g;
    g.id = QStringLiteral("board");
    g.desktopMode = true;
    g.anchor = PageAnchor::Top;
    g.offset.x = PageDim::pixels(0);
    g.offset.y = PageDim::pixels(0);
    g.size.x = width.isSet() ? width : defaultSpeakBoardWidth();
    g.size.y = PageDim::expression(heightExpr);
    g.rows = rows;
    g.columns = cols;
    g.rowTracks = starTracks(weights);
    g.gapPx = gap;
    g.marginPx = margin;
    QColor bg = theme.bgMain.isValid() ? theme.bgMain : QColor(10, 10, 11);
    bg.setAlpha(255);
    g.style.background = bg;
    if (theme.border.isValid()) {
        g.style.borderColor = theme.border;
    }
    g.style.radius = PageBox::all(PageChrome::kDefaultRadius);
    g.style.thickness = PageBox::all(PageChrome::kDefaultThickness);
    doc.grids.push_back(std::move(g));
}

} // namespace SettingsPageBuild
} // namespace gazer
