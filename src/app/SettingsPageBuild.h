#pragma once

#include "layout/PageDim.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>

namespace gazer {
namespace SettingsPageBuild {

inline PageAction command(const QString& name)
{
    PageAction a;
    a.type = PageActionType::Command;
    a.command = name;
    return a;
}

inline PageCell cell(const QString& id, const QString& label, int row, int col,
                     const QString& commandName, const QColor& bg, int colSpan = 1,
                     bool interactive = true, const QString& role = {},
                     const QString& caption = {}, const QString& icon = {})
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
    c.interactive = interactive;
    if (bg.isValid()) {
        c.style.background = bg;
        c.style.foreground = ThemeColors::contrastOn(bg);
    }
    if (interactive && !commandName.isEmpty()) {
        c.actions.push_back(command(commandName));
    }
    return c;
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
    g.style.radius = 8.0;
    g.style.thickness = 1.0;
    doc.grids.push_back(std::move(g));
}

} // namespace SettingsPageBuild
} // namespace gazer
