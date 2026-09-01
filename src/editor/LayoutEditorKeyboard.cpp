#include "editor/LayoutEditorKeyboard.h"

#include "layout/PageEdit.h"

namespace gazer {

namespace {

PageAction sendKey(const QString& key)
{
    PageAction a;
    a.type = PageActionType::Send;
    a.sendKey = key;
    return a;
}

PageAction openPage(const QString& id)
{
    PageAction a;
    a.type = PageActionType::Nav;
    a.verb = PageVerb::Open;
    a.targetKind = PageTargetKind::Page;
    a.targetScope = PageNavScope::Id;
    a.targetId = id;
    return a;
}

PageAction commandAction(const QString& name)
{
    PageAction a;
    a.type = PageActionType::Command;
    a.command = name;
    return a;
}

PageCell keyCell(const QString& id, const QString& label, int row, int col, const QString& send)
{
    PageCell c;
    c.id = id;
    c.label = label;
    c.row = row;
    c.col = col;
    c.actions.push_back(sendKey(send));
    return c;
}

PageGrid boardGrid(int cols, int rows, int width, int height)
{
    PageGrid g;
    g.id = QStringLiteral("board");
    g.desktopMode = true;
    g.anchor = PageAnchor::Bottom;
    g.size.x = PageDim::pixels(width);
    g.size.y = PageDim::pixels(height);
    g.rows = rows;
    g.columns = cols;
    g.gapPx = 6;
    g.marginPx = 8;
    return g;
}

} // namespace

PageDocument makeBlankDocument()
{
    PageDocument d;
    d.id = QStringLiteral("untitled");
    d.name = QStringLiteral("Untitled");
    d.autoClose = true;
    d.grids.push_back(boardGrid(4, 2, 800, 280));
    return d;
}

PageDocument makeKeyboardPage(const QString& id, const QString& name)
{
    PageDocument d;
    d.id = id;
    d.name = name;
    d.autoClose = true;
    PageGrid g = boardGrid(10, 4, 1100, 360);
    const QString row1 = QStringLiteral("qwertyuiop");
    const QString row2 = QStringLiteral("asdfghjkl");
    const QString row3 = QStringLiteral("zxcvbnm");
    auto addRow = [&](const QString& keys, int row, int col0) {
        for (int i = 0; i < keys.size(); ++i) {
            const QString ch = keys.mid(i, 1);
            g.cells.push_back(keyCell(QStringLiteral("k_%1_%2").arg(row).arg(i), ch, row, col0 + i,
                                      ch));
        }
    };
    addRow(row1, 0, 0);
    addRow(row2, 1, 0);
    addRow(row3, 2, 0);
    PageCell space;
    space.id = QStringLiteral("space");
    space.label = QStringLiteral("Space");
    space.row = 3;
    space.col = 2;
    space.colSpan = 5;
    space.actions.push_back(sendKey(QStringLiteral("Space")));
    g.cells.push_back(space);
    PageCell shift;
    shift.id = QStringLiteral("shift");
    shift.label = QStringLiteral("Shift");
    shift.icon = QStringLiteral("Shift");
    shift.row = 3;
    shift.col = 0;
    shift.colSpan = 2;
    shift.activeState = QStringLiteral("mod.shift");
    shift.actions.push_back(commandAction(QStringLiteral("leftShift")));
    g.cells.push_back(shift);
    PageCell bk;
    bk.id = QStringLiteral("back");
    bk.label = QStringLiteral("Bksp");
    bk.row = 3;
    bk.col = 7;
    bk.colSpan = 3;
    bk.actions.push_back(sendKey(QStringLiteral("Backspace")));
    g.cells.push_back(bk);
    d.grids.push_back(std::move(g));
    return d;
}

PageDocument makeTemplateDocument(EditorTemplate tmpl, const QString& id, const QString& name)
{
    if (tmpl == EditorTemplate::Keyboard) {
        const QString boardId = id.trimmed().isEmpty() ? QStringLiteral("untitled") : id.trimmed();
        const QString boardName = name.trimmed().isEmpty() ? boardId : name.trimmed();
        return makeKeyboardPage(boardId, boardName);
    }

    PageDocument d = makeBlankDocument();
    d.id = id.trimmed().isEmpty() ? QStringLiteral("untitled") : id.trimmed();
    d.name = name.trimmed().isEmpty() ? d.id : name.trimmed();
    PageEdit::ensurePrimaryGrid(d);
    PageGrid* g = PageEdit::primaryGrid(d);

    switch (tmpl) {
    case EditorTemplate::KeyboardRow: {
        g->columns = 10;
        g->rows = 1;
        g->gapPx = 4;
        g->marginPx = 6;
        g->size.x = PageDim::pixels(1100);
        g->size.y = PageDim::pixels(96);
        const QString keys = QStringLiteral("qwertyuiop");
        for (int i = 0; i < keys.size(); ++i) {
            g->cells.push_back(keyCell(QStringLiteral("k_%1").arg(keys.mid(i, 1)), keys.mid(i, 1), 0,
                                       i, keys.mid(i, 1)));
        }
        break;
    }
    case EditorTemplate::SettingsRow: {
        g->columns = 4;
        g->rows = 1;
        g->gapPx = 10;
        g->marginPx = 10;
        g->size.x = PageDim::pixels(1080);
        g->size.y = PageDim::pixels(140);
        const QStringList labels = {QStringLiteral("Keyboard"), QStringLiteral("Mouse"),
                                    QStringLiteral("Assist"), QStringLiteral("Settings")};
        const QStringList pages = {QStringLiteral("uw_qwerty"), QStringLiteral("example_mouse"),
                                   QStringLiteral("example_assist"),
                                   QStringLiteral("main_settings")};
        const QStringList icons = {QStringLiteral("Keyboard"), QStringLiteral("Mouse"),
                                   QStringLiteral("Conversation"), QStringLiteral("SizeAndPosition")};
        for (int i = 0; i < labels.size(); ++i) {
            PageCell c;
            c.id = QStringLiteral("open_%1").arg(i);
            c.label = labels[i];
            c.icon = icons[i];
            c.row = 0;
            c.col = i;
            c.actions.push_back(openPage(pages[i]));
            g->cells.push_back(c);
        }
        break;
    }
    case EditorTemplate::EdgeChip: {
        d.grids.clear();
        PageZone z;
        z.id = QStringLiteral("edge");
        z.label = QStringLiteral("Main");
        z.anchor = PageAnchor::Bottom;
        z.size.x = PageDim::pixels(300);
        z.size.y = PageDim::pixels(150);
        z.dwellOffset.y = PageDim::pixels(240);
        z.dwellSize.x = PageDim::pixels(300);
        z.dwellSize.y = PageDim::pixels(200);
        PageAction a;
        a.type = PageActionType::Nav;
        a.verb = PageVerb::Open;
        a.targetKind = PageTargetKind::Grid;
        a.targetScope = PageNavScope::Id;
        a.targetId = QStringLiteral("drawer");
        z.actions.push_back(a);
        d.zones.push_back(z);
        break;
    }
    case EditorTemplate::Blank:
    case EditorTemplate::Keyboard:
        break;
    }

    return d;
}

} // namespace gazer
