#include "editor/LayoutEditorSession.h"

#include "layout/PageEdit.h"

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
    case EditorItemKind::Zone:
    case EditorItemKind::Button:
        break;
    }
    return {};
}

PageAction sendLetter(const QString& label)
{
    PageAction a;
    a.type = PageActionType::Send;
    a.sendKey = label.toLower() == QLatin1String("button") ? QString() : label;
    return a;
}

PageAction commandAction(const QString& name)
{
    PageAction a;
    a.type = PageActionType::Command;
    a.command = name;
    return a;
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
    QString gridId;
    if (const PageGrid* own = PageEdit::gridOwningCell(document(), m_sel.itemId)) {
        gridId = own->id;
        if (!PageEdit::findEmptyCell(*own, row, col)) {
            row = own->rows;
            col = 0;
        }
    } else if (!PageEdit::findEmptyCell(document(), row, col)) {
        const PageGrid* g = PageEdit::primaryGrid(document());
        row = g ? g->rows : 0;
        col = 0;
    }
    addItemAt(kind, row, col, gridId);
}

void LayoutEditorSession::addZoneAt(const QPoint& virtTopLeft)
{
    PageZone z;
    z.id = uniqueItemId(QStringLiteral("zone"));
    z.label = QStringLiteral("Zone");
    z.anchor = PageAnchor::TopLeft;
    z.offset.x = PageDim::pixels(virtTopLeft.x());
    z.offset.y = PageDim::pixels(virtTopLeft.y());
    z.size.x = PageDim::pixels(200);
    z.size.y = PageDim::pixels(120);
    z.dwellSize = z.size;
    const QString newId = z.id;
    edit(QStringLiteral("Add zone"), [&](PageDocument& d) { d.zones.push_back(z); });
    m_placeKind.reset();
    emit placeKindChanged();
    selectItem(newId);
}

void LayoutEditorSession::addItemAt(EditorItemKind kind, int row, int col, const QString& gridId)
{
    if (kind == EditorItemKind::Zone) {
        addZoneAt(QPoint(200, 200));
        return;
    }
    PageCell cell;
    cell.id = uniqueItemId(QStringLiteral("item"));
    cell.label = QStringLiteral("Button");
    cell.role = roleForKind(kind);
    cell.row = qMax(0, row);
    cell.col = qMax(0, col);
    if (kind == EditorItemKind::Label || kind == EditorItemKind::Slider) {
        cell.interactive = false;
        if (kind == EditorItemKind::Label) {
            cell.label = QStringLiteral("Label");
        }
    }
    if (kind == EditorItemKind::Toggle) {
        cell.actions.push_back(commandAction(QStringLiteral("toggleDwellSuspend")));
        cell.dwellExempt = true;
    } else if (cell.interactive) {
        cell.actions.push_back(sendLetter(cell.label));
    }
    const QString newId = cell.id;
    const QString gid = gridId;
    edit(QStringLiteral("Add %1").arg(cell.label), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* g = gid.isEmpty() ? PageEdit::primaryGrid(d) : PageEdit::findGrid(d, gid);
        if (!g) {
            g = PageEdit::primaryGrid(d);
        }
        PageEdit::expandForCell(*g, cell);
        g->cells.push_back(cell);
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
    QVector<EditorClip> copies;
    QStringList newIds;
    QStringList reserved;
    auto nextId = [&](const QString& stem) {
        QString id = uniqueItemId(stem);
        int n = 2;
        while (reserved.contains(id)) {
            id = QStringLiteral("%1_%2").arg(stem).arg(n++);
        }
        reserved.push_back(id);
        return id;
    };
    for (const QString& id : m_sel.itemIds) {
        if (const PageCell* src = PageEdit::findCell(document(), id)) {
            EditorClip clip;
            clip.kind = EditorClip::Kind::Cell;
            clip.cell = *src;
            clip.cell.id = nextId(src->id);
            clip.cell.col = src->col + qMax(1, src->colSpan);
            if (const PageGrid* g = PageEdit::gridOwningCell(document(), id)) {
                clip.gridId = g->id;
            }
            newIds.push_back(clip.cell.id);
            copies.push_back(clip);
        } else if (const PageZone* src = PageEdit::findZone(document(), id)) {
            EditorClip clip;
            clip.kind = EditorClip::Kind::Zone;
            clip.zone = *src;
            clip.zone.id = nextId(src->id);
            if (clip.zone.offset.x.isSet()) {
                clip.zone.offset.x.value += 16;
            }
            if (clip.zone.offset.y.isSet()) {
                clip.zone.offset.y.value += 16;
            }
            newIds.push_back(clip.zone.id);
            copies.push_back(clip);
        }
    }
    if (copies.isEmpty()) {
        return;
    }
    edit(QStringLiteral("Duplicate items"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        for (EditorClip& copy : copies) {
            if (copy.kind == EditorClip::Kind::Cell) {
                PageGrid* g = copy.gridId.isEmpty() ? PageEdit::primaryGrid(d)
                                                    : PageEdit::findGrid(d, copy.gridId);
                if (!g) {
                    g = PageEdit::primaryGrid(d);
                }
                PageEdit::expandForCell(*g, copy.cell);
                g->cells.push_back(copy.cell);
            } else {
                d.zones.push_back(copy.zone);
            }
        }
    });
    setSelection({EditorTarget::Item, newIds.first(), newIds});
}

void LayoutEditorSession::deleteSelected()
{
    if (m_sel.target == EditorTarget::Grid && !m_sel.itemId.isEmpty()) {
        const QString id = m_sel.itemId;
        edit(QStringLiteral("Delete grid"), [&](PageDocument& d) { PageEdit::removeGrid(d, id); });
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    if (m_sel.target == EditorTarget::Style && !m_sel.itemId.isEmpty()) {
        const QString id = m_sel.itemId;
        edit(QStringLiteral("Delete style"), [&](PageDocument& d) { d.styles.remove(id); });
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    if (m_sel.target == EditorTarget::Dwell && !m_sel.itemId.isEmpty()) {
        const QString id = m_sel.itemId;
        edit(QStringLiteral("Delete dwell"), [&](PageDocument& d) { d.dwells.remove(id); });
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    if (m_sel.target != EditorTarget::Item || m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Delete items"), [&](PageDocument& d) {
        for (const QString& id : ids) {
            PageEdit::removeLeaf(d, id);
        }
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
        if (const PageCell* src = PageEdit::findCell(document(), id)) {
            EditorClip clip;
            clip.kind = EditorClip::Kind::Cell;
            clip.cell = *src;
            if (const PageGrid* g = PageEdit::gridOwningCell(document(), id)) {
                clip.gridId = g->id;
            }
            m_clipboard.push_back(clip);
        } else if (const PageZone* src = PageEdit::findZone(document(), id)) {
            EditorClip clip;
            clip.kind = EditorClip::Kind::Zone;
            clip.zone = *src;
            m_clipboard.push_back(clip);
        }
    }
    if (!m_clipboard.isEmpty()) {
        emit statusMessage(QStringLiteral("Copied %1 cell(s)/zone(s)").arg(m_clipboard.size()));
    }
}

void LayoutEditorSession::pasteClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    int row = 0;
    int col = 0;
    const bool haveCell = PageEdit::findEmptyCell(document(), row, col);
    QStringList reserved;
    QStringList newIds;
    QVector<EditorClip> copies;
    auto nextId = [&](const QString& stem) {
        QString id = uniqueItemId(stem);
        int n = 2;
        while (reserved.contains(id)) {
            id = QStringLiteral("%1_%2").arg(stem).arg(n++);
        }
        reserved.push_back(id);
        return id;
    };
    for (EditorClip item : m_clipboard) {
        if (item.kind == EditorClip::Kind::Cell) {
            item.cell.id = nextId(item.cell.id);
            if (haveCell) {
                item.cell.row = row;
                item.cell.col = col;
                ++col;
                const PageGrid* g = PageEdit::primaryGrid(document());
                const int cols = g ? g->columns : 4;
                if (col >= cols) {
                    col = 0;
                    ++row;
                }
            }
            newIds.push_back(item.cell.id);
        } else {
            item.zone.id = nextId(item.zone.id);
            newIds.push_back(item.zone.id);
        }
        copies.push_back(item);
    }
    edit(QStringLiteral("Paste items"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        for (const EditorClip& item : copies) {
            if (item.kind == EditorClip::Kind::Cell) {
                PageGrid* g = item.gridId.isEmpty() ? PageEdit::primaryGrid(d)
                                                    : PageEdit::findGrid(d, item.gridId);
                if (!g) {
                    g = PageEdit::primaryGrid(d);
                }
                PageEdit::expandForCell(*g, item.cell);
                g->cells.push_back(item.cell);
            } else {
                d.zones.push_back(item.zone);
            }
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
    edit(QStringLiteral("Move %1").arg(itemId), [&](PageDocument& d) {
        if (PageCell* c = PageEdit::findCell(d, itemId)) {
            c->row = row;
            c->col = col;
            if (PageGrid* g = PageEdit::gridOwningCell(d, itemId)) {
                PageEdit::expandForCell(*g, *c);
            }
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
    edit(QStringLiteral("Nudge items"), [&](PageDocument& d) {
        for (const QString& id : ids) {
            if (PageCell* c = PageEdit::findCell(d, id)) {
                c->row = qMax(0, c->row + dRow);
                c->col = qMax(0, c->col + dCol);
                if (PageGrid* g = PageEdit::gridOwningCell(d, id)) {
                    PageEdit::expandForCell(*g, *c);
                }
            } else if (PageZone* z = PageEdit::findZone(d, id)) {
                const double x0 = z->offset.x.isSet() ? z->offset.x.value : 0.0;
                const double y0 = z->offset.y.isSet() ? z->offset.y.value : 0.0;
                z->offset.x = PageDim::pixels(x0 + double(dCol * px));
                z->offset.y = PageDim::pixels(y0 + double(dRow * px));
            }
        }
    });
}

void LayoutEditorSession::setItemLabel(const QString& itemId, const QString& label)
{
    edit(QStringLiteral("Rename"), [&](PageDocument& d) {
        if (PageLeaf* leaf = PageEdit::findLeaf(d, itemId)) {
            leaf->label = label;
        }
    });
}

void LayoutEditorSession::addTopGrid()
{
    PageGrid g;
    g.id = uniqueItemId(QStringLiteral("grid"));
    g.desktopMode = true;
    g.anchor = PageAnchor::Bottom;
    g.size.x = PageDim::pixels(800);
    g.size.y = PageDim::pixels(280);
    g.rows = 1;
    g.columns = 4;
    g.gapPx = 8;
    g.marginPx = 8;
    const QString id = g.id;
    edit(QStringLiteral("Add grid"), [&](PageDocument& d) { d.grids.push_back(g); });
    selectGrid(id);
}

void LayoutEditorSession::addSubGrid()
{
    const QString parentId = selectedGridId();
    PageGrid sub;
    sub.id = uniqueItemId(QStringLiteral("sub"));
    sub.nested = true;
    sub.rows = 1;
    sub.columns = 2;
    sub.rowSpan = 1;
    sub.colSpan = 1;
    const QString id = sub.id;
    edit(QStringLiteral("Add subgrid"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* parent = parentId.isEmpty() ? PageEdit::primaryGrid(d)
                                              : PageEdit::findGrid(d, parentId);
        if (!parent) {
            parent = PageEdit::primaryGrid(d);
        }
        if (!parent) {
            return;
        }
        int row = 0;
        int col = 0;
        if (!PageEdit::findEmptyCell(*parent, row, col)) {
            row = parent->rows;
            col = 0;
            parent->rows += 1;
        }
        sub.row = row;
        sub.col = col;
        parent->subGrids.push_back(sub);
    });
    selectGrid(id);
}

void LayoutEditorSession::addNamedStyle()
{
    const QString id = uniqueItemId(QStringLiteral("style"));
    edit(QStringLiteral("Add style"), [&](PageDocument& d) { d.styles.insert(id, PageChrome{}); });
    setSelection({EditorTarget::Style, id, {}});
}

void LayoutEditorSession::addNamedDwell()
{
    const QString id = uniqueItemId(QStringLiteral("dwell"));
    edit(QStringLiteral("Add dwell"), [&](PageDocument& d) { d.dwells.insert(id, PageDwell{}); });
    setSelection({EditorTarget::Dwell, id, {}});
}

void LayoutEditorSession::addGridRow()
{
    const QString gid = selectedGridId();
    edit(QStringLiteral("Add row"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* g = gid.isEmpty() ? PageEdit::primaryGrid(d) : PageEdit::findGrid(d, gid);
        if (!g) {
            g = PageEdit::primaryGrid(d);
        }
        if (g) {
            g->rows += 1;
        }
    });
}

void LayoutEditorSession::addGridColumn()
{
    const QString gid = selectedGridId();
    edit(QStringLiteral("Add column"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* g = gid.isEmpty() ? PageEdit::primaryGrid(d) : PageEdit::findGrid(d, gid);
        if (!g) {
            g = PageEdit::primaryGrid(d);
        }
        if (g) {
            g->columns += 1;
        }
    });
}

void LayoutEditorSession::packGrid()
{
    edit(QStringLiteral("Pack grid"), [](PageDocument& d) { PageEdit::ensureGridFits(d); });
}

void LayoutEditorSession::equalizeSelectedWidths()
{
    if (m_sel.itemIds.size() < 2) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Equalize widths"), [&](PageDocument& d) {
        int n = 0;
        for (const QString& id : ids) {
            if (PageEdit::findCell(d, id)) {
                ++n;
            }
        }
        if (n < 2) {
            return;
        }
        for (const QString& id : ids) {
            if (PageCell* c = PageEdit::findCell(d, id)) {
                c->colSpan = 1;
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
    edit(QStringLiteral("Align row"), [&](PageDocument& d) {
        int row = -1;
        for (const QString& id : ids) {
            if (const PageCell* c = PageEdit::findCell(d, id)) {
                row = (row < 0) ? c->row : qMin(row, c->row);
            }
        }
        if (row < 0) {
            return;
        }
        for (const QString& id : ids) {
            if (PageCell* c = PageEdit::findCell(d, id)) {
                c->row = row;
            }
        }
    });
}

void LayoutEditorSession::applyChromeToSelected(const std::function<void(PageChrome&)>& mut,
                                               const QString& undoLabel)
{
    const QStringList ids = m_sel.itemIds;
    if (ids.isEmpty()) {
        return;
    }
    edit(undoLabel, [&](PageDocument& d) {
        for (const QString& id : ids) {
            if (PageLeaf* leaf = PageEdit::findLeaf(d, id)) {
                mut(leaf->style);
            }
        }
    });
}

void LayoutEditorSession::setActions(const QString& itemId, QVector<PageAction> acts)
{
    const QStringList ids = actionItemIds(itemId);
    if (ids.isEmpty()) {
        return;
    }
    edit(QStringLiteral("Actions"), [&](PageDocument& d) {
        for (const QString& id : ids) {
            if (PageLeaf* leaf = PageEdit::findLeaf(d, id)) {
                leaf->actions = acts;
            }
        }
    });
}

void LayoutEditorSession::snapWindowTo(const QPoint& virtualTopLeft, const QSize& virtualScreen)
{
    const int x = virtualTopLeft.x();
    const int y = virtualTopLeft.y();
    const int vw = qMax(1, virtualScreen.width());
    const int vh = qMax(1, virtualScreen.height());
    const QString gid = selectedGridId();
    edit(QStringLiteral("Move window"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* g = gid.isEmpty() ? PageEdit::primaryGrid(d) : PageEdit::findGrid(d, gid);
        if (!g) {
            g = PageEdit::primaryGrid(d);
        }
        if (!g || g->nested) {
            return;
        }
        const int w = g->size.x.isSet() ? int(g->size.x.resolve(vw, vh)) : 800;
        const int h = g->size.y.isSet() ? int(g->size.y.resolve(vh, vh)) : 280;
        const bool cx = qAbs(x - (vw - w) / 2) < 48;
        const bool cy = qAbs(y - (vh - h) / 2) < 48;
        if (y + h > vh - 40 && cx) {
            g->anchor = PageAnchor::Bottom;
            g->offset = {};
        } else if (y < 40 && cx) {
            g->anchor = PageAnchor::Top;
            g->offset = {};
        } else if (cx && cy) {
            g->anchor = PageAnchor::Center;
            g->offset = {};
        } else {
            g->anchor = PageAnchor::TopLeft;
            g->offset.x = PageDim::pixels(x);
            g->offset.y = PageDim::pixels(y);
        }
    });
}

void LayoutEditorSession::resizeFreeItem(const QString& itemId, const PageDim& width,
                                         const PageDim& height)
{
    edit(QStringLiteral("Resize %1").arg(itemId), [&](PageDocument& d) {
        if (PageZone* z = PageEdit::findZone(d, itemId)) {
            z->size.x = width;
            z->size.y = height;
        }
    });
}

void LayoutEditorSession::raiseSelected()
{
    if (m_sel.itemIds.isEmpty()) {
        return;
    }
    const QStringList ids = m_sel.itemIds;
    edit(QStringLiteral("Bring forward"), [&](PageDocument& d) {
        PageEdit::forEachGrid(d, [&](PageGrid& g) {
            for (int i = g.cells.size() - 2; i >= 0; --i) {
                if (ids.contains(g.cells[i].id) && !ids.contains(g.cells[i + 1].id)) {
                    qSwap(g.cells[i], g.cells[i + 1]);
                }
            }
        });
        for (int i = d.zones.size() - 2; i >= 0; --i) {
            if (ids.contains(d.zones[i].id) && !ids.contains(d.zones[i + 1].id)) {
                qSwap(d.zones[i], d.zones[i + 1]);
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
    edit(QStringLiteral("Send backward"), [&](PageDocument& d) {
        PageEdit::forEachGrid(d, [&](PageGrid& g) {
            for (int i = 1; i < g.cells.size(); ++i) {
                if (ids.contains(g.cells[i].id) && !ids.contains(g.cells[i - 1].id)) {
                    qSwap(g.cells[i], g.cells[i - 1]);
                }
            }
        });
        for (int i = 1; i < d.zones.size(); ++i) {
            if (ids.contains(d.zones[i].id) && !ids.contains(d.zones[i - 1].id)) {
                qSwap(d.zones[i], d.zones[i - 1]);
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
    edit(QStringLiteral("Convert to zone"), [&](PageDocument& d) {
        for (const QString& id : ids) {
            PageCell* c = PageEdit::findCell(d, id);
            if (!c) {
                continue;
            }
            PageZone z = PageEdit::zoneFromCell(*c);
            PageEdit::removeLeaf(d, id);
            d.zones.push_back(z);
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
    const bool have = PageEdit::findEmptyCell(document(), row, col);
    edit(QStringLiteral("Convert to cell"), [&](PageDocument& d) {
        PageEdit::ensurePrimaryGrid(d);
        PageGrid* g = PageEdit::primaryGrid(d);
        for (const QString& id : ids) {
            PageZone* z = PageEdit::findZone(d, id);
            if (!z) {
                continue;
            }
            PageCell c = PageEdit::cellFromZone(*z);
            if (have) {
                c.row = row;
                c.col = col;
                ++col;
                if (col >= g->columns) {
                    col = 0;
                    ++row;
                }
            }
            PageEdit::removeLeaf(d, id);
            PageEdit::expandForCell(*g, c);
            g->cells.push_back(c);
        }
    });
}

void LayoutEditorSession::resizeItem(const QString& itemId, int rowSpan, int colSpan,
                                     double /*widthUnits*/)
{
    edit(QStringLiteral("Resize %1").arg(itemId), [&](PageDocument& d) {
        if (PageCell* c = PageEdit::findCell(d, itemId)) {
            c->rowSpan = qMax(1, rowSpan);
            c->colSpan = qMax(1, colSpan);
            if (PageGrid* g = PageEdit::gridOwningCell(d, itemId)) {
                PageEdit::expandForCell(*g, *c);
            }
        }
    });
}

QStringList LayoutEditorSession::validate(const QStringList& catalogIds) const
{
    QStringList issues;
    const PageDocument& doc = document();
    if (doc.id.trimmed().isEmpty()) {
        issues.push_back(QStringLiteral("Page id is empty"));
    }
    QSet<QString> seen;
    auto checkAction = [&](const PageAction& a, const QString& where) {
        if (a.type == PageActionType::Command && a.command.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: command name is empty").arg(where));
        }
        if (a.type == PageActionType::Nav && a.targetScope == PageNavScope::Id
            && a.targetId.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: page target is empty").arg(where));
        }
        if (a.type == PageActionType::Nav && a.targetKind == PageTargetKind::Page
            && a.targetScope == PageNavScope::Id && !a.targetId.isEmpty() && !catalogIds.isEmpty()
            && !catalogIds.contains(a.targetId) && a.targetId != doc.id) {
            issues.push_back(QStringLiteral("%1: unknown page '%2'").arg(where, a.targetId));
        }
        if (a.type == PageActionType::Speak && a.speakText.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: speak text is empty").arg(where));
        }
        if (a.type == PageActionType::Send && a.sendKey.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1: send key is empty").arg(where));
        }
    };
    auto checkLeaf = [&](const PageLeaf& leaf, const QString& kind) {
        if (leaf.id.trimmed().isEmpty()) {
            issues.push_back(QStringLiteral("%1 has an empty id").arg(kind));
            return;
        }
        if (seen.contains(leaf.id)) {
            issues.push_back(QStringLiteral("Duplicate id '%1'").arg(leaf.id));
        }
        seen.insert(leaf.id);
        if (leaf.interactive && leaf.actions.isEmpty()) {
            issues.push_back(QStringLiteral("%1 '%2' has no actions").arg(kind, leaf.id));
        }
        for (const PageAction& a : leaf.actions) {
            checkAction(a, leaf.id);
        }
    };
    PageEdit::forEachGrid(doc, [&](const PageGrid& g) {
        for (const PageCell& c : g.cells) {
            checkLeaf(c, QStringLiteral("Cell"));
        }
    });
    for (const PageZone& z : doc.zones) {
        checkLeaf(z, QStringLiteral("Zone"));
    }
    return issues;
}

} // namespace gazer
