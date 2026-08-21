#include "editor/LayoutEditorSession.h"

#include <QSet>
#include <algorithm>

namespace gazer {

namespace {

QString roleForKind(EditorItemKind kind)
{
    switch (kind) {
    case EditorItemKind::Label:
        return QStringLiteral("label");
    case EditorItemKind::Toggle:
        return QStringLiteral("toggle");
    case EditorItemKind::Tab:
        return QStringLiteral("tab");
    case EditorItemKind::Slider:
        return QStringLiteral("slider");
    case EditorItemKind::Unbounded:
    case EditorItemKind::Button:
        break;
    }
    return {};
}

} // namespace

QStringList LayoutEditorSession::actionItemIds(const QString& itemId) const
{
    if (m_sel.target == EditorTarget::Item && !m_sel.itemIds.isEmpty()) {
        return m_sel.itemIds;
    }
    if (!itemId.isEmpty()) {
        return {itemId};
    }
    return {};
}

void LayoutEditorSession::addItem(EditorItemKind kind)
{
    int row = 0;
    int col = 0;
    if (!findEmptyCell(row, col)) {
        row = document().grid.rows;
        col = 0;
    }
    addItemAt(kind, row, col);
}

void LayoutEditorSession::addFreeItemAt(const QPoint& virtTopLeft)
{
    LayoutItem item;
    item.id = uniqueItemId(QStringLiteral("free"));
    item.label = QStringLiteral("Free");
    item.setAnchor(LayoutDwellRegion::ScreenAnchor::TopLeft);
    item.dwellRegion.x = DimSpec::pixels(virtTopLeft.x());
    item.dwellRegion.y = DimSpec::pixels(virtTopLeft.y());
    const QString newId = item.id;
    edit(QStringLiteral("Add free item"), [&](LayoutDocument& d) { d.items.push_back(item); });
    m_placeKind.reset();
    emit placeKindChanged();
    selectItem(newId);
}

void LayoutEditorSession::addItemAt(EditorItemKind kind, int row, int col)
{
    const QString stem = kind == EditorItemKind::Unbounded ? QStringLiteral("free")
                                                           : QStringLiteral("item");
    LayoutItem item;
    item.id = uniqueItemId(stem);
    item.label = kind == EditorItemKind::Unbounded ? QStringLiteral("Free")
                                                   : QStringLiteral("Button");
    item.role = roleForKind(kind);
    item.applyKind();
    if (kind == EditorItemKind::Label || kind == EditorItemKind::Slider) {
        item.interactive = false;
    }
    if (kind == EditorItemKind::Unbounded) {
        item.setAnchor(LayoutDwellRegion::ScreenAnchor::Bottom);
        item.label = QStringLiteral("Free");
    } else {
        item.row = qMax(0, row);
        item.col = qMax(0, col);
        item.action.type = LayoutAction::Type::TypeText;
        item.action.text = item.label.toLower() == QLatin1String("button") ? QString()
                                                                           : item.label;
        if (kind == EditorItemKind::Toggle) {
            item.action.type = LayoutAction::Type::Command;
            item.action.name = QStringLiteral("toggleDwellSuspend");
            item.action.text.clear();
        }
        item.actions = {item.action};
    }
    const QString newId = item.id;
    edit(QStringLiteral("Add %1").arg(item.label), [&](LayoutDocument& d) {
        d.items.push_back(item);
        expandGridForItem(d, item);
    });
    m_placeKind.reset();
    emit placeKindChanged();
    selectItem(newId);
}

void LayoutEditorSession::duplicateSelected()
{
    if (m_sel.target != EditorTarget::Item || m_sel.itemIds.isEmpty()) {
        return;
    }
    QVector<LayoutItem> copies;
    QStringList newIds;
    QStringList reserved;
    auto nextId = [&](const QString& stem) {
        QString id = uniqueItemId(stem);
        int n = 2;
        while (reserved.contains(id)) {
            id = QStringLiteral("%1_%2").arg(stem).arg(n++);
            if (!document().findItem(id) && !reserved.contains(id)) {
                break;
            }
        }
        reserved.push_back(id);
        return id;
    };
    for (const QString& id : m_sel.itemIds) {
        const LayoutItem* src = itemById(id);
        if (!src) {
            continue;
        }
        LayoutItem copy = *src;
        copy.id = nextId(src->id);
        if (copy.participatesInBoardGrid()) {
            copy.col = src->col + src->colSpan;
            if (copy.col >= document().grid.columns) {
                copy.col = 0;
                copy.row = src->row + src->rowSpan;
            }
        } else if (copy.dwellRegion.x.isSet() || copy.dwellRegion.y.isSet()) {
            const double x0 = copy.dwellRegion.x.isSet() ? copy.dwellRegion.x.value : 0.0;
            const double y0 = copy.dwellRegion.y.isSet() ? copy.dwellRegion.y.value : 0.0;
            copy.dwellRegion.x = DimSpec::pixels(x0 + 16);
            copy.dwellRegion.y = DimSpec::pixels(y0 + 16);
        }
        newIds.push_back(copy.id);
        copies.push_back(copy);
    }
    if (copies.isEmpty()) {
        return;
    }
    edit(QStringLiteral("Duplicate items"), [&](LayoutDocument& d) {
        for (const LayoutItem& copy : copies) {
            d.items.push_back(copy);
            expandGridForItem(d, copy);
        }
    });
    setSelection({EditorTarget::Item, newIds.first(), newIds});
}

void LayoutEditorSession::deleteSelected()
{
    if (m_sel.target != EditorTarget::Item || m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Delete items"), [&](LayoutDocument& d) {
        d.items.erase(std::remove_if(d.items.begin(), d.items.end(),
                                     [&](const LayoutItem& it) { return ids.contains(it.id); }),
                      d.items.end());
    });
    setSelection({EditorTarget::Document, {}, {}});
}

void LayoutEditorSession::cutSelected()
{
    copySelected();
    deleteSelected();
}

void LayoutEditorSession::copySelected()
{
    m_clipboard.clear();
    for (const QString& id : m_sel.itemIds) {
        if (const LayoutItem* src = itemById(id)) {
            m_clipboard.push_back(*src);
        }
    }
    if (!m_clipboard.isEmpty()) {
        emit statusMessage(QStringLiteral("Copied %1 item(s)").arg(m_clipboard.size()));
    }
}

void LayoutEditorSession::pasteClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    QVector<LayoutItem> copies;
    QStringList newIds;
    int row = 0;
    int col = 0;
    const bool haveCell = findEmptyCell(row, col);
    QStringList reserved;
    auto nextId = [&](const QString& stem) {
        QString id = uniqueItemId(stem);
        int n = 2;
        while (reserved.contains(id)) {
            id = QStringLiteral("%1_%2").arg(stem).arg(n++);
            if (!document().findItem(id) && !reserved.contains(id)) {
                break;
            }
        }
        reserved.push_back(id);
        return id;
    };
    for (LayoutItem item : m_clipboard) {
        item.id = nextId(item.id);
        if (item.participatesInBoardGrid()) {
            if (haveCell) {
                item.row = row;
                item.col = col;
                ++col;
                if (col >= document().grid.columns) {
                    col = 0;
                    ++row;
                }
            } else {
                item.row = document().grid.rows;
                item.col = 0;
            }
        }
        newIds.push_back(item.id);
        copies.push_back(item);
    }
    edit(QStringLiteral("Paste items"), [&](LayoutDocument& d) {
        for (const LayoutItem& item : copies) {
            d.items.push_back(item);
            expandGridForItem(d, item);
        }
    });
    if (!newIds.isEmpty()) {
        setSelection({EditorTarget::Item, newIds.first(), newIds});
    }
}

void LayoutEditorSession::moveItemToCell(const QString& itemId, int row, int col)
{
    row = qMax(0, row);
    col = qMax(0, col);
    edit(QStringLiteral("Move %1").arg(itemId), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (it.id != itemId) {
                continue;
            }
            it.row = row;
            it.col = col;
            expandGridForItem(d, it);
            break;
        }
    });
}

void LayoutEditorSession::moveSelected(int dRow, int dCol)
{
    nudgeSelected(dRow, dCol, 4);
}

void LayoutEditorSession::nudgeSelected(int dRow, int dCol, int freePx)
{
    if (m_sel.itemIds.isEmpty() || (dRow == 0 && dCol == 0)) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    const int px = qMax(1, freePx);
    edit(QStringLiteral("Nudge items"), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (!ids.contains(it.id)) {
                continue;
            }
            if (it.participatesInBoardGrid()) {
                it.row = qMax(0, it.row + dRow);
                it.col = qMax(0, it.col + dCol);
                expandGridForItem(d, it);
            } else {
                const double x0 = it.dwellRegion.x.isSet() ? it.dwellRegion.x.value : 0.0;
                const double y0 = it.dwellRegion.y.isSet() ? it.dwellRegion.y.value : 0.0;
                it.dwellRegion.x = DimSpec::pixels(x0 + double(dCol * px));
                it.dwellRegion.y = DimSpec::pixels(y0 + double(dRow * px));
            }
        }
    });
}

void LayoutEditorSession::setItemLabel(const QString& itemId, const QString& label)
{
    edit(QStringLiteral("Rename"), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (it.id == itemId) {
                it.label = label;
                break;
            }
        }
    });
}

void LayoutEditorSession::addGridRow()
{
    edit(QStringLiteral("Add row"), [](LayoutDocument& d) { d.grid.rows += 1; });
}

void LayoutEditorSession::addGridColumn()
{
    edit(QStringLiteral("Add column"), [](LayoutDocument& d) { d.grid.columns += 1; });
}

void LayoutEditorSession::packGrid()
{
    edit(QStringLiteral("Pack grid"), [](LayoutDocument& d) {
        int maxRow = 0;
        int maxCol = 0;
        for (const LayoutItem& it : d.items) {
            if (!it.participatesInBoardGrid()) {
                continue;
            }
            maxRow = qMax(maxRow, it.row + it.rowSpan - 1);
            maxCol = qMax(maxCol, it.col + it.colSpan - 1);
        }
        d.grid.rows = qMax(1, maxRow + 1);
        d.grid.columns = qMax(1, maxCol + 1);
    });
}

void LayoutEditorSession::equalizeSelectedWidths()
{
    if (m_sel.itemIds.size() < 2) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Equalize widths"), [&](LayoutDocument& d) {
        double total = 0;
        int n = 0;
        for (const LayoutItem& it : d.items) {
            if (ids.contains(it.id) && it.participatesInBoardGrid()) {
                total += it.widthUnits > 0 ? it.widthUnits : 1.0;
                ++n;
            }
        }
        if (n < 1) {
            return;
        }
        const double u = total / n;
        d.grid.unitRows = true;
        for (LayoutItem& it : d.items) {
            if (ids.contains(it.id) && it.participatesInBoardGrid()) {
                it.widthUnits = u;
            }
        }
    });
}

void LayoutEditorSession::alignSelectedRow()
{
    if (m_sel.itemIds.size() < 2) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Align row"), [&](LayoutDocument& d) {
        int row = -1;
        for (const LayoutItem& it : d.items) {
            if (ids.contains(it.id) && it.participatesInBoardGrid()) {
                row = (row < 0) ? it.row : qMin(row, it.row);
            }
        }
        if (row < 0) {
            return;
        }
        for (LayoutItem& it : d.items) {
            if (ids.contains(it.id) && it.participatesInBoardGrid()) {
                it.row = row;
            }
        }
    });
}

void LayoutEditorSession::applyChromeToSelected(const std::function<void(LayoutChromeStyle&)>& mut,
                                               const QString& undoLabel)
{
    const QStringList ids = m_sel.itemIds;
    if (ids.isEmpty()) {
        return;
    }
    edit(undoLabel, [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (ids.contains(it.id)) {
                mut(it.style);
            }
        }
    });
}

void LayoutEditorSession::setActions(const QString& itemId, QVector<LayoutAction> acts)
{
    const QStringList ids = actionItemIds(itemId);
    if (ids.isEmpty()) {
        return;
    }
    edit(QStringLiteral("Actions"), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (!ids.contains(it.id)) {
                continue;
            }
            it.actions = acts;
            it.action = it.actions.isEmpty() ? LayoutAction{} : it.actions.first();
        }
    });
}

void LayoutEditorSession::snapWindowTo(const QPoint& virtualTopLeft, const QSize& virtualScreen)
{
    const int x = virtualTopLeft.x();
    const int y = virtualTopLeft.y();
    const int vw = qMax(1, virtualScreen.width());
    const int vh = qMax(1, virtualScreen.height());
    edit(QStringLiteral("Move window"), [&](LayoutDocument& d) {
        d.placement.specified = true;
        d.placement.x = DimSpec::pixels(x);
        d.placement.y = DimSpec::pixels(y);
        const int w = d.placement.width.resolveInt(vw, 800);
        const int h = d.placement.height.resolveInt(vh, 280);
        using A = LayoutWindowPlacement::Anchor;
        const bool cx = qAbs(x - (vw - w) / 2) < 48;
        const bool cy = qAbs(y - (vh - h) / 2) < 48;
        if (y + h > vh - 40 && cx) {
            d.placement.anchor = A::BottomCenter;
            d.placement.x = {};
            d.placement.y = {};
        } else if (y < 40 && cx) {
            d.placement.anchor = A::TopCenter;
            d.placement.x = {};
            d.placement.y = {};
        } else if (cx && cy) {
            d.placement.anchor = A::Center;
            d.placement.x = {};
            d.placement.y = {};
        } else {
            d.placement.anchor = A::Default;
        }
    });
}

void LayoutEditorSession::resizeFreeItem(const QString& itemId, const DimSpec& width,
                                         const DimSpec& height)
{
    edit(QStringLiteral("Resize %1").arg(itemId), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (it.id != itemId) {
                continue;
            }
            it.dwellRegion.width = width;
            it.dwellRegion.height = height;
            break;
        }
    });
}

void LayoutEditorSession::raiseSelected()
{
    if (m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Bring forward"), [&](LayoutDocument& d) {
        for (int i = d.items.size() - 2; i >= 0; --i) {
            if (ids.contains(d.items[i].id) && !ids.contains(d.items[i + 1].id)) {
                qSwap(d.items[i], d.items[i + 1]);
            }
        }
    });
}

void LayoutEditorSession::lowerSelected()
{
    if (m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Send backward"), [&](LayoutDocument& d) {
        for (int i = 1; i < d.items.size(); ++i) {
            if (ids.contains(d.items[i].id) && !ids.contains(d.items[i - 1].id)) {
                qSwap(d.items[i], d.items[i - 1]);
            }
        }
    });
}

void LayoutEditorSession::convertSelectedToFree()
{
    if (m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Convert to free"), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (!ids.contains(it.id) || it.isUnbounded()) {
                continue;
            }
            it.setAnchor(LayoutDwellRegion::ScreenAnchor::BottomCenter);
        }
    });
}

void LayoutEditorSession::convertSelectedToCell()
{
    if (m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    int row = 0;
    int col = 0;
    const bool have = findEmptyCell(row, col);
    edit(QStringLiteral("Convert to cell"), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (!ids.contains(it.id) || !it.isUnbounded()) {
                continue;
            }
            it.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::None;
            if (have) {
                it.row = row;
                it.col = col;
                ++col;
                if (col >= d.grid.columns) {
                    col = 0;
                    ++row;
                }
            }
            expandGridForItem(d, it);
        }
    });
}

void LayoutEditorSession::resizeItem(const QString& itemId, int rowSpan, int colSpan,
                                     double widthUnits)
{
    edit(QStringLiteral("Resize %1").arg(itemId), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (it.id != itemId) {
                continue;
            }
            it.rowSpan = qMax(1, rowSpan);
            it.colSpan = qMax(1, colSpan);
            it.widthUnits = qMax(0.0, widthUnits);
            expandGridForItem(d, it);
            break;
        }
    });
}

void LayoutEditorSession::ensureGridFits(LayoutDocument& doc)
{
    int maxRow = 0;
    int maxCol = 0;
    for (const LayoutItem& it : doc.items) {
        if (!it.participatesInBoardGrid()) {
            continue;
        }
        maxRow = qMax(maxRow, it.row + it.rowSpan - 1);
        maxCol = qMax(maxCol, it.col + it.colSpan - 1);
    }
    doc.grid.rows = qMax(doc.grid.rows, maxRow + 1);
    doc.grid.columns = qMax(doc.grid.columns, maxCol + 1);
    if (doc.grid.rows < 1) {
        doc.grid.rows = 1;
    }
    if (doc.grid.columns < 1) {
        doc.grid.columns = 1;
    }
}

void LayoutEditorSession::expandGridForItem(LayoutDocument& doc, const LayoutItem& item)
{
    if (!item.participatesInBoardGrid()) {
        return;
    }
    doc.grid.rows = qMax(doc.grid.rows, item.row + item.rowSpan);
    doc.grid.columns = qMax(doc.grid.columns, item.col + item.colSpan);
}

bool LayoutEditorSession::findEmptyCell(int& row, int& col) const
{
    const LayoutDocument& doc = document();
    const int rows = qMax(1, doc.grid.rows);
    const int cols = qMax(1, doc.grid.columns);
    QVector<QVector<bool>> used(rows, QVector<bool>(cols, false));
    for (const LayoutItem& it : doc.items) {
        if (!it.participatesInBoardGrid()) {
            continue;
        }
        for (int r = it.row; r < it.row + it.rowSpan && r < rows; ++r) {
            for (int c = it.col; c < it.col + it.colSpan && c < cols; ++c) {
                if (r >= 0 && c >= 0) {
                    used[r][c] = true;
                }
            }
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!used[r][c]) {
                row = r;
                col = c;
                return true;
            }
        }
    }
    return false;
}

QStringList LayoutEditorSession::validate(const QStringList& catalogIds) const
{
    QStringList issues;
    const LayoutDocument& doc = document();
    if (doc.id.trimmed().isEmpty()) {
        issues.push_back(QStringLiteral("Board id is empty"));
    }
    QSet<QString> seen;
    const int rows = qMax(1, doc.grid.rows);
    const int cols = qMax(1, doc.grid.columns);
    QVector<QVector<QString>> occ(rows, QVector<QString>(cols));
    auto checkAction = [&](const LayoutAction& a, const QString& where) {
        if (a.type == LayoutAction::Type::Command && a.name.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: command name is empty").arg(where));
        }
        if ((a.type == LayoutAction::Type::OpenLayout || a.type == LayoutAction::Type::LoadLayout)
            && a.layoutId.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: layout id is empty").arg(where));
        }
        if ((a.type == LayoutAction::Type::OpenLayout || a.type == LayoutAction::Type::LoadLayout)
            && !a.layoutId.isEmpty() && !catalogIds.isEmpty() && !catalogIds.contains(a.layoutId)
            && a.layoutId != doc.id) {
            issues.push_back(
                QStringLiteral("%1: unknown layout '%2'").arg(where, a.layoutId));
        }
        if ((a.type == LayoutAction::Type::Speak || a.type == LayoutAction::Type::TypeText)
            && a.text.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: text is empty").arg(where));
        }
        if (a.type == LayoutAction::Type::Script && a.source.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: script is empty").arg(where));
        }
    };
    auto checkActs = [&](const QVector<LayoutAction>& acts, const QString& where) {
        for (int i = 0; i < acts.size(); ++i) {
            checkAction(acts[i], acts.size() == 1 ? where
                                                  : QStringLiteral("%1 step %2").arg(where).arg(i + 1));
        }
    };
    checkActs(doc.onOpen, QStringLiteral("onOpen"));
    checkActs(doc.onLoad, QStringLiteral("onLoad"));
    checkActs(doc.onClose, QStringLiteral("onClose"));
    for (const LayoutChildRef& ch : doc.children) {
        if (ch.id.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("Child slot id is empty"));
        }
        if (ch.layoutId.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("Child '%1' has no layout").arg(ch.id));
        } else if (!catalogIds.isEmpty() && !catalogIds.contains(ch.layoutId)) {
            issues.push_back(
                QStringLiteral("Child '%1' unknown layout '%2'").arg(ch.id, ch.layoutId));
        }
    }
    for (const LayoutItem& it : doc.items) {
        if (it.id.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("An item has an empty id"));
            continue;
        }
        if (seen.contains(it.id)) {
            issues.push_back(QStringLiteral("Duplicate item id '%1'").arg(it.id));
        }
        seen.insert(it.id);
        checkActs(it.effectiveActions(), it.label.isEmpty() ? it.id : it.label);
        if (!it.participatesInBoardGrid()) {
            continue;
        }
        for (int r = it.row; r < it.row + it.rowSpan; ++r) {
            for (int c = it.col; c < it.col + it.colSpan; ++c) {
                if (r < 0 || c < 0 || r >= rows || c >= cols) {
                    issues.push_back(
                        QStringLiteral("%1 sits outside the grid (r%2 c%3)").arg(it.id).arg(r).arg(c));
                    continue;
                }
                if (!occ[r][c].isEmpty()) {
                    issues.push_back(QStringLiteral("%1 overlaps %2 at r%3 c%4")
                                         .arg(it.id, occ[r][c])
                                         .arg(r)
                                         .arg(c));
                } else {
                    occ[r][c] = it.id;
                }
            }
        }
    }
    issues.removeDuplicates();
    return issues;
}

} // namespace gazer
