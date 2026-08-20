#include "editor/LayoutEditorKeyboard.h"

namespace gazer {

LayoutDocument makeBlankDocument()
{
    LayoutDocument d;
    d.schemaVersion = 1;
    d.id = QStringLiteral("untitled");
    d.name = QStringLiteral("Untitled");
    d.autoClose = true;
    d.grid.columns = 4;
    d.grid.rows = 2;
    d.grid.gapPx = 8;
    d.grid.marginPx = 8;
    d.placement.specified = true;
    d.placement.anchor = LayoutWindowPlacement::Anchor::BottomCenter;
    d.placement.width = DimSpec::pixels(800);
    d.placement.height = DimSpec::pixels(280);
    return d;
}

QVector<EditorLayer> makeBlankLayers()
{
    return {{QStringLiteral("Base"), {}, makeBlankDocument()}};
}

QVector<EditorLayer> makeTemplateLayers(EditorTemplate tmpl, const QString& id, const QString& name)
{
    if (tmpl == EditorTemplate::Keyboard) {
        const QString boardId = id.trimmed().isEmpty() ? QStringLiteral("untitled") : id.trimmed();
        const QString boardName = name.trimmed().isEmpty() ? boardId : name.trimmed();
        const auto family = makeKeyboardFamily(boardId, boardName);
        return {{QStringLiteral("Base"), {}, family[0]},
                {QStringLiteral("Shift"), QStringLiteral("_shift"), family[1]},
                {QStringLiteral("Symbols"), QStringLiteral("_sym"), family[2]}};
    }

    LayoutDocument d = makeBlankDocument();
    d.id = id.trimmed().isEmpty() ? QStringLiteral("untitled") : id.trimmed();
    d.name = name.trimmed().isEmpty() ? d.id : name.trimmed();
    d.placement.specified = true;
    d.placement.anchor = LayoutWindowPlacement::Anchor::BottomCenter;

    auto addKey = [&](const QString& label, int col, double u) {
        LayoutItem it;
        it.id = QStringLiteral("k_%1").arg(label);
        it.label = label;
        it.row = 0;
        it.col = col;
        it.widthUnits = u;
        it.action.type = LayoutAction::Type::TypeText;
        it.action.text = label;
        it.actions = {it.action};
        d.items.push_back(it);
    };

    switch (tmpl) {
    case EditorTemplate::KeyboardRow: {
        d.grid.columns = 10;
        d.grid.rows = 1;
        d.grid.gapPx = 4;
        d.grid.marginPx = 6;
        d.grid.unitRows = true;
        d.placement.width = DimSpec::pixels(1100);
        d.placement.height = DimSpec::pixels(96);
        const QString keys = QStringLiteral("qwertyuiop");
        for (int i = 0; i < keys.size(); ++i) {
            addKey(keys.mid(i, 1), i, 1.0);
        }
        break;
    }
    case EditorTemplate::SettingsRow: {
        d.grid.columns = 4;
        d.grid.rows = 1;
        d.grid.gapPx = 10;
        d.grid.marginPx = 10;
        d.placement.width = DimSpec::pixels(1080);
        d.placement.height = DimSpec::pixels(140);
        const QStringList labels = {QStringLiteral("Keyboard"), QStringLiteral("Mouse"),
                                    QStringLiteral("Assist"), QStringLiteral("Settings")};
        const QStringList layouts = {QStringLiteral("example_keyboard"),
                                     QStringLiteral("example_mouse"),
                                     QStringLiteral("example_assist"),
                                     QStringLiteral("main_settings_button_timing")};
        for (int i = 0; i < labels.size(); ++i) {
            LayoutItem it;
            it.id = QStringLiteral("open_%1").arg(i);
            it.label = labels[i];
            it.row = 0;
            it.col = i;
            it.action.type = LayoutAction::Type::OpenLayout;
            it.action.layoutId = layouts[i];
            it.actions = {it.action};
            d.items.push_back(it);
        }
        break;
    }
    case EditorTemplate::EdgeChip: {
        d.placement.hidden = true;
        d.grid.columns = 1;
        d.grid.rows = 1;
        LayoutItem it;
        it.id = QStringLiteral("edge");
        it.label = QStringLiteral("Main");
        it.unbounded = true;
        it.hasDwellRegion = true;
        it.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Bottom;
        it.dwellRegion.width = DimSpec::pixels(160);
        it.dwellRegion.height = DimSpec::pixels(48);
        it.action.type = LayoutAction::Type::Command;
        it.action.name = QStringLiteral("expandMaster");
        it.actions = {it.action};
        d.items.push_back(it);
        break;
    }
    case EditorTemplate::Blank:
    case EditorTemplate::Keyboard:
        break;
    }

    return {{QStringLiteral("Base"), {}, std::move(d)}};
}


namespace {

LayoutChromeStyle modStyle()
{
    LayoutChromeStyle s;
    s.background = QColor(QStringLiteral("#3d4f66"));
    s.foreground = QColor(QStringLiteral("#ffffff"));
    return s;
}

LayoutChromeStyle accentStyle()
{
    LayoutChromeStyle s;
    s.background = QColor(QStringLiteral("#0a7ea4"));
    s.foreground = QColor(QStringLiteral("#ffffff"));
    return s;
}

LayoutItem letter(const QString& ch, int row, int col, double u)
{
    LayoutItem it;
    it.id = QStringLiteral("k_%1_%2_%3").arg(row).arg(col).arg(ch);
    it.label = ch;
    it.row = row;
    it.col = col;
    it.widthUnits = u;
    it.action.type = LayoutAction::Type::TypeText;
    it.action.text = ch;
    it.actions = {it.action};
    return it;
}

LayoutItem cmdKey(const QString& id, const QString& label, int row, int col, double u,
                  const QString& command, bool accent = false)
{
    LayoutItem it;
    it.id = id;
    it.label = label;
    it.row = row;
    it.col = col;
    it.widthUnits = u;
    it.action.type = LayoutAction::Type::Command;
    it.action.name = command;
    it.actions = {it.action};
    it.style = accent ? accentStyle() : modStyle();
    return it;
}

LayoutItem layerKey(const QString& id, const QString& label, int row, int col, double u,
                    const QString& layoutId, bool accent = false)
{
    LayoutItem it;
    it.id = id;
    it.label = label;
    it.row = row;
    it.col = col;
    it.widthUnits = u;
    it.action.type = LayoutAction::Type::LoadLayout;
    it.action.layoutId = layoutId;
    it.actions = {it.action};
    it.style = accent ? accentStyle() : modStyle();
    return it;
}

LayoutDocument board(const QString& id, const QString& name)
{
    LayoutDocument d;
    d.schemaVersion = 1;
    d.id = id;
    d.name = name;
    d.autoClose = true;
    d.grid.columns = 12;
    d.grid.rows = 3;
    d.grid.gapPx = 4;
    d.grid.marginPx = 4;
    d.grid.unitRows = true;
    d.placement.specified = true;
    d.placement.anchor = LayoutWindowPlacement::Anchor::BottomCenter;
    d.placement.width = DimSpec::pixels(1280);
    d.placement.height = DimSpec::pixels(280);
    return d;
}

} // namespace

QVector<LayoutDocument> makeKeyboardFamily(const QString& id, const QString& name)
{
    const QString shiftId = id + QStringLiteral("_shift");
    const QString symId = id + QStringLiteral("_sym");

    LayoutDocument base = board(id, name);
    const QString row0 = QStringLiteral("qwertyuiop");
    base.items.push_back(cmdKey(QStringLiteral("tab"), QStringLiteral("Tab"), 0, 0, 0.9,
                                QStringLiteral("tab")));
    for (int i = 0; i < row0.size(); ++i) {
        base.items.push_back(letter(row0.mid(i, 1), 0, i + 1, i < 5 ? 0.95 : 1.0));
    }
    base.items.push_back(cmdKey(QStringLiteral("bksp"), QStringLiteral("⌫"), 0, 11, 1.2,
                                QStringLiteral("backspace")));
    base.items.push_back(layerKey(QStringLiteral("shift"), QStringLiteral("⇧"), 1, 0, 1.3, shiftId));
    const QString row1 = QStringLiteral("asdfghjkl");
    for (int i = 0; i < row1.size(); ++i) {
        base.items.push_back(letter(row1.mid(i, 1), 1, i + 1, 1.0));
    }
    base.items.push_back(letter(QStringLiteral(","), 1, 10, 0.9));
    base.items.push_back(letter(QStringLiteral("'"), 1, 11, 0.9));
    base.items.push_back(cmdKey(QStringLiteral("cmd"), QStringLiteral("⌘"), 2, 0, 0.9,
                                QStringLiteral("escape")));
    base.items.push_back(layerKey(QStringLiteral("sym"), QStringLiteral("123?"), 2, 1, 0.9, symId, true));
    const QString row2 = QStringLiteral("zxcv");
    for (int i = 0; i < row2.size(); ++i) {
        base.items.push_back(letter(row2.mid(i, 1), 2, i + 2, 0.9));
    }
    base.items.push_back(cmdKey(QStringLiteral("space"), QStringLiteral("␣"), 2, 6, 1.5,
                                QStringLiteral("space")));
    base.items.push_back(letter(QStringLiteral("b"), 2, 7, 0.9));
    base.items.push_back(letter(QStringLiteral("n"), 2, 8, 1.0));
    base.items.push_back(letter(QStringLiteral("m"), 2, 9, 1.0));
    base.items.push_back(letter(QStringLiteral("."), 2, 10, 0.9));
    base.items.push_back(cmdKey(QStringLiteral("enter"), QStringLiteral("↵"), 2, 11, 1.2,
                                QStringLiteral("enter"), true));

    LayoutDocument shift = board(shiftId, name + QStringLiteral(" (shift)"));
    shift.items.push_back(cmdKey(QStringLiteral("tab"), QStringLiteral("Tab"), 0, 0, 0.9,
                                 QStringLiteral("tab")));
    const QString s0 = QStringLiteral("QWERTYUIOP");
    for (int i = 0; i < s0.size(); ++i) {
        shift.items.push_back(letter(s0.mid(i, 1), 0, i + 1, i < 5 ? 0.95 : 1.0));
    }
    shift.items.push_back(cmdKey(QStringLiteral("bksp"), QStringLiteral("⌫"), 0, 11, 1.2,
                                 QStringLiteral("backspace")));
    shift.items.push_back(layerKey(QStringLiteral("shift"), QStringLiteral("⇧"), 1, 0, 1.3, id, true));
    const QString s1 = QStringLiteral("ASDFGHJKL");
    for (int i = 0; i < s1.size(); ++i) {
        shift.items.push_back(letter(s1.mid(i, 1), 1, i + 1, 1.0));
    }
    shift.items.push_back(letter(QStringLiteral("<"), 1, 10, 0.9));
    shift.items.push_back(letter(QStringLiteral("\""), 1, 11, 0.9));
    shift.items.push_back(cmdKey(QStringLiteral("cmd"), QStringLiteral("⌘"), 2, 0, 0.9,
                                 QStringLiteral("escape")));
    shift.items.push_back(layerKey(QStringLiteral("sym"), QStringLiteral("123?"), 2, 1, 0.9, symId, true));
    const QString s2 = QStringLiteral("ZXCV");
    for (int i = 0; i < s2.size(); ++i) {
        shift.items.push_back(letter(s2.mid(i, 1), 2, i + 2, 0.9));
    }
    shift.items.push_back(cmdKey(QStringLiteral("space"), QStringLiteral("␣"), 2, 6, 1.5,
                                 QStringLiteral("space")));
    shift.items.push_back(letter(QStringLiteral("B"), 2, 7, 0.9));
    shift.items.push_back(letter(QStringLiteral("N"), 2, 8, 1.0));
    shift.items.push_back(letter(QStringLiteral("M"), 2, 9, 1.0));
    shift.items.push_back(letter(QStringLiteral("?"), 2, 10, 0.9));
    shift.items.push_back(cmdKey(QStringLiteral("enter"), QStringLiteral("↵"), 2, 11, 1.2,
                                 QStringLiteral("enter"), true));

    LayoutDocument sym = board(symId, name + QStringLiteral(" (sym)"));
    const QString n0 = QStringLiteral("1234567890");
    sym.items.push_back(cmdKey(QStringLiteral("tab"), QStringLiteral("Tab"), 0, 0, 0.9,
                               QStringLiteral("tab")));
    for (int i = 0; i < n0.size(); ++i) {
        sym.items.push_back(letter(n0.mid(i, 1), 0, i + 1, 1.0));
    }
    sym.items.push_back(cmdKey(QStringLiteral("bksp"), QStringLiteral("⌫"), 0, 11, 1.2,
                               QStringLiteral("backspace")));
    sym.items.push_back(layerKey(QStringLiteral("shift"), QStringLiteral("⇧"), 1, 0, 1.3, shiftId));
    const QString n1[] = {QStringLiteral("-"), QStringLiteral("/"), QStringLiteral(":"),
                          QStringLiteral(";"), QStringLiteral("("), QStringLiteral(")"),
                          QStringLiteral("$"), QStringLiteral("&"), QStringLiteral("@")};
    for (int i = 0; i < 9; ++i) {
        sym.items.push_back(letter(n1[i], 1, i + 1, 1.0));
    }
    sym.items.push_back(letter(QStringLiteral("\""), 1, 10, 0.9));
    sym.items.push_back(letter(QStringLiteral("!"), 1, 11, 0.9));
    sym.items.push_back(layerKey(QStringLiteral("abc"), QStringLiteral("ABC"), 2, 0, 0.9, id, true));
    const QString n2[] = {QStringLiteral("."), QStringLiteral(","), QStringLiteral("?"),
                          QStringLiteral("'"), QStringLiteral("#")};
    for (int i = 0; i < 5; ++i) {
        sym.items.push_back(letter(n2[i], 2, i + 1, 0.9));
    }
    sym.items.push_back(cmdKey(QStringLiteral("space"), QStringLiteral("␣"), 2, 6, 1.5,
                               QStringLiteral("space")));
    sym.items.push_back(letter(QStringLiteral("+"), 2, 7, 0.9));
    sym.items.push_back(letter(QStringLiteral("="), 2, 8, 1.0));
    sym.items.push_back(letter(QStringLiteral("*"), 2, 9, 1.0));
    sym.items.push_back(letter(QStringLiteral("%"), 2, 10, 0.9));
    sym.items.push_back(cmdKey(QStringLiteral("enter"), QStringLiteral("↵"), 2, 11, 1.2,
                               QStringLiteral("enter"), true));

    return {base, shift, sym};
}

} // namespace gazer
