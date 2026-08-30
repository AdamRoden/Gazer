#include "editor/LayoutEditorMenu.h"

#include "editor/LayoutEditorSession.h"

#include <QMenu>

namespace gazer {

void execEditorItemMenu(QWidget* parent, const QPoint& globalPos, LayoutEditorSession& session,
                        EditorItemMenuOpts opts)
{
    QMenu menu(parent);
    auto* dup = menu.addAction(QStringLiteral("Duplicate"));
    auto* del = menu.addAction(QStringLiteral("Delete"));
    QAction* addSub = nullptr;
    if (opts.subgrid) {
        menu.addSeparator();
        addSub = menu.addAction(QStringLiteral("Add subgrid"));
    }
    auto* toZone = menu.addAction(QStringLiteral("Convert to zone"));
    auto* toCell = menu.addAction(QStringLiteral("Convert to cell"));
    QAction* raise = nullptr;
    QAction* lower = nullptr;
    if (opts.zOrder) {
        menu.addSeparator();
        raise = menu.addAction(QStringLiteral("Bring forward"));
        lower = menu.addAction(QStringLiteral("Send backward"));
    }

    const EditorTarget target = session.selection().target;
    const bool item = target == EditorTarget::Item;
    const bool grid = target == EditorTarget::Grid;
    const bool named = target == EditorTarget::Style || target == EditorTarget::Dwell;
    dup->setEnabled(item);
    del->setEnabled(item || grid || named);
    if (addSub) {
        addSub->setEnabled(grid || item);
    }
    toZone->setEnabled(item);
    toCell->setEnabled(item);
    if (raise) {
        raise->setEnabled(item);
    }
    if (lower) {
        lower->setEnabled(item);
    }

    QAction* chosen = menu.exec(globalPos);
    if (chosen == dup) {
        session.duplicateSelected();
    } else if (chosen == del) {
        session.deleteSelected();
    } else if (addSub && chosen == addSub) {
        session.addSubGrid();
    } else if (chosen == toZone) {
        session.convertSelectedToFree();
    } else if (chosen == toCell) {
        session.convertSelectedToCell();
    } else if (raise && chosen == raise) {
        session.raiseSelected();
    } else if (lower && chosen == lower) {
        session.lowerSelected();
    }
}

} // namespace gazer
