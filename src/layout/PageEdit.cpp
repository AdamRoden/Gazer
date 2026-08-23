#include "layout/PageEdit.h"

#include <algorithm>
#include <functional>

namespace gazer {
namespace PageEdit {

namespace {

PageGrid* findGridIn(QVector<PageGrid>& nodes, const QString& id)
{
    for (PageGrid& g : nodes) {
        if (!id.isEmpty() && g.id == id) {
            return &g;
        }
        if (PageGrid* nested = findGridIn(g.subGrids, id)) {
            return nested;
        }
    }
    return nullptr;
}

const PageGrid* findGridIn(const QVector<PageGrid>& nodes, const QString& id)
{
    for (const PageGrid& g : nodes) {
        if (!id.isEmpty() && g.id == id) {
            return &g;
        }
        if (const PageGrid* nested = findGridIn(g.subGrids, id)) {
            return nested;
        }
    }
    return nullptr;
}

PageCell* findCellIn(QVector<PageGrid>& nodes, const QString& id)
{
    for (PageGrid& g : nodes) {
        for (PageCell& c : g.cells) {
            if (c.id == id) {
                return &c;
            }
        }
        if (PageCell* nested = findCellIn(g.subGrids, id)) {
            return nested;
        }
    }
    return nullptr;
}

const PageCell* findCellIn(const QVector<PageGrid>& nodes, const QString& id)
{
    for (const PageGrid& g : nodes) {
        for (const PageCell& c : g.cells) {
            if (c.id == id) {
                return &c;
            }
        }
        if (const PageCell* nested = findCellIn(g.subGrids, id)) {
            return nested;
        }
    }
    return nullptr;
}

void walkGrids(QVector<PageGrid>& nodes, const std::function<void(PageGrid&)>& fn)
{
    for (PageGrid& g : nodes) {
        fn(g);
        walkGrids(g.subGrids, fn);
    }
}

void walkGrids(const QVector<PageGrid>& nodes, const std::function<void(const PageGrid&)>& fn)
{
    for (const PageGrid& g : nodes) {
        fn(g);
        walkGrids(g.subGrids, fn);
    }
}

} // namespace

PageGrid* primaryGrid(PageDocument& doc)
{
    if (doc.grids.isEmpty()) {
        return nullptr;
    }
    return &doc.grids[0];
}

const PageGrid* primaryGrid(const PageDocument& doc)
{
    if (doc.grids.isEmpty()) {
        return nullptr;
    }
    return &doc.grids[0];
}

PageGrid* findGrid(PageDocument& doc, const QString& id)
{
    return findGridIn(doc.grids, id);
}

const PageGrid* findGrid(const PageDocument& doc, const QString& id)
{
    return findGridIn(doc.grids, id);
}

PageCell* findCell(PageDocument& doc, const QString& id)
{
    return findCellIn(doc.grids, id);
}

const PageCell* findCell(const PageDocument& doc, const QString& id)
{
    return findCellIn(doc.grids, id);
}

PageZone* findZone(PageDocument& doc, const QString& id)
{
    for (PageZone& z : doc.zones) {
        if (z.id == id) {
            return &z;
        }
    }
    return nullptr;
}

const PageZone* findZone(const PageDocument& doc, const QString& id)
{
    for (const PageZone& z : doc.zones) {
        if (z.id == id) {
            return &z;
        }
    }
    return nullptr;
}

PageLeaf* findLeaf(PageDocument& doc, const QString& id)
{
    if (PageCell* c = findCell(doc, id)) {
        return c;
    }
    return findZone(doc, id);
}

const PageLeaf* findLeaf(const PageDocument& doc, const QString& id)
{
    if (const PageCell* c = findCell(doc, id)) {
        return c;
    }
    return findZone(doc, id);
}

bool isZone(const PageDocument& doc, const QString& id)
{
    return findZone(doc, id) != nullptr;
}

QStringList allIds(const PageDocument& doc)
{
    QStringList ids;
    walkGrids(doc.grids, [&](const PageGrid& g) {
        if (!g.id.isEmpty()) {
            ids.push_back(g.id);
        }
        for (const PageCell& c : g.cells) {
            if (!c.id.isEmpty()) {
                ids.push_back(c.id);
            }
        }
    });
    for (const PageZone& z : doc.zones) {
        if (!z.id.isEmpty()) {
            ids.push_back(z.id);
        }
    }
    return ids;
}

void forEachGrid(PageDocument& doc, const std::function<void(PageGrid&)>& fn)
{
    walkGrids(doc.grids, fn);
}

void forEachGrid(const PageDocument& doc, const std::function<void(const PageGrid&)>& fn)
{
    walkGrids(doc.grids, fn);
}

void forEachCell(PageDocument& doc, const std::function<void(PageGrid&, PageCell&)>& fn)
{
    walkGrids(doc.grids, [&](PageGrid& g) {
        for (PageCell& c : g.cells) {
            fn(g, c);
        }
    });
}

void ensurePrimaryGrid(PageDocument& doc)
{
    if (!doc.grids.isEmpty()) {
        return;
    }
    PageGrid g;
    g.id = QStringLiteral("board");
    g.desktopMode = true;
    g.anchor = PageAnchor::Bottom;
    g.size.x = PageDim::pixels(800);
    g.size.y = PageDim::pixels(280);
    g.rows = 2;
    g.columns = 4;
    g.gapPx = 8;
    g.marginPx = 8;
    doc.grids.push_back(std::move(g));
}

void expandForCell(PageGrid& grid, const PageCell& cell)
{
    grid.rows = qMax(grid.rows, cell.row + qMax(1, cell.rowSpan));
    grid.columns = qMax(grid.columns, cell.col + qMax(1, cell.colSpan));
}

void ensureGridFits(PageDocument& doc)
{
    ensurePrimaryGrid(doc);
    walkGrids(doc.grids, [](PageGrid& g) {
        int maxRow = 0;
        int maxCol = 0;
        for (const PageCell& c : g.cells) {
            maxRow = qMax(maxRow, c.row + qMax(1, c.rowSpan) - 1);
            maxCol = qMax(maxCol, c.col + qMax(1, c.colSpan) - 1);
        }
        g.rows = qMax(qMax(1, g.rows), maxRow + 1);
        g.columns = qMax(qMax(1, g.columns), maxCol + 1);
    });
}

bool findEmptyCell(const PageGrid& grid, int& row, int& col)
{
    const int rows = qMax(1, grid.rows);
    const int cols = qMax(1, grid.columns);
    QVector<QVector<bool>> used(rows, QVector<bool>(cols, false));
    for (const PageCell& c : grid.cells) {
        for (int r = c.row; r < c.row + qMax(1, c.rowSpan) && r < rows; ++r) {
            for (int co = c.col; co < c.col + qMax(1, c.colSpan) && co < cols; ++co) {
                if (r >= 0 && co >= 0) {
                    used[r][co] = true;
                }
            }
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int co = 0; co < cols; ++co) {
            if (!used[r][co]) {
                row = r;
                col = co;
                return true;
            }
        }
    }
    return false;
}

bool findEmptyCell(const PageDocument& doc, int& row, int& col)
{
    const PageGrid* g = primaryGrid(doc);
    return g && findEmptyCell(*g, row, col);
}

PageGrid* gridOwningCell(PageDocument& doc, const QString& cellId)
{
    PageGrid* found = nullptr;
    walkGrids(doc.grids, [&](PageGrid& g) {
        if (found) {
            return;
        }
        for (const PageCell& c : g.cells) {
            if (c.id == cellId) {
                found = &g;
                return;
            }
        }
    });
    return found;
}

const PageGrid* gridOwningCell(const PageDocument& doc, const QString& cellId)
{
    const PageGrid* found = nullptr;
    walkGrids(doc.grids, [&](const PageGrid& g) {
        if (found) {
            return;
        }
        for (const PageCell& c : g.cells) {
            if (c.id == cellId) {
                found = &g;
                return;
            }
        }
    });
    return found;
}

void remapStyleId(PageDocument& doc, const QString& from, const QString& to)
{
    if (from.isEmpty() || from == to) {
        return;
    }
    walkGrids(doc.grids, [&](PageGrid& g) {
        if (g.styleId == from) {
            g.styleId = to;
        }
        for (PageCell& c : g.cells) {
            if (c.styleId == from) {
                c.styleId = to;
            }
        }
    });
    for (PageZone& z : doc.zones) {
        if (z.styleId == from) {
            z.styleId = to;
        }
    }
}

void remapDwellId(PageDocument& doc, const QString& from, const QString& to)
{
    if (from.isEmpty() || from == to) {
        return;
    }
    walkGrids(doc.grids, [&](PageGrid& g) {
        if (g.dwellId == from) {
            g.dwellId = to;
        }
        for (PageCell& c : g.cells) {
            if (c.dwellId == from) {
                c.dwellId = to;
            }
        }
    });
    for (PageZone& z : doc.zones) {
        if (z.dwellId == from) {
            z.dwellId = to;
        }
    }
}

void remapPageActionTargets(PageDocument& doc, const QHash<QString, QString>& idMap)
{
    auto remapActs = [&](QVector<PageAction>& acts) {
        for (PageAction& a : acts) {
            if (a.type != PageActionType::Page || a.targetKind != PageTargetKind::Page) {
                continue;
            }
            const auto it = idMap.constFind(a.targetId);
            if (it != idMap.cend()) {
                a.targetId = it.value();
            }
        }
    };
    walkGrids(doc.grids, [&](PageGrid& g) {
        for (PageCell& c : g.cells) {
            remapActs(c.actions);
        }
    });
    for (PageZone& z : doc.zones) {
        remapActs(z.actions);
    }
}

PageZone zoneFromCell(const PageCell& cell)
{
    PageZone z;
    static_cast<PageLeaf&>(z) = cell;
    z.anchor = PageAnchor::Bottom;
    z.aboveTaskbar = true;
    z.size.x = PageDim::pixels(200);
    z.size.y = PageDim::pixels(120);
    z.dwellSize = z.size;
    return z;
}

PageCell cellFromZone(const PageZone& zone)
{
    PageCell c;
    static_cast<PageLeaf&>(c) = zone;
    c.rowSpan = 1;
    c.colSpan = 1;
    return c;
}

void removeLeaf(PageDocument& doc, const QString& id)
{
    auto eraseZone = [&]() {
        doc.zones.erase(std::remove_if(doc.zones.begin(), doc.zones.end(),
                                       [&](const PageZone& z) { return z.id == id; }),
                        doc.zones.end());
    };
    walkGrids(doc.grids, [&](PageGrid& g) {
        g.cells.erase(std::remove_if(g.cells.begin(), g.cells.end(),
                                     [&](const PageCell& c) { return c.id == id; }),
                      g.cells.end());
    });
    eraseZone();
}

void removeGrid(PageDocument& doc, const QString& id)
{
    if (id.isEmpty()) {
        return;
    }
    std::function<bool(QVector<PageGrid>&)> erase = [&](QVector<PageGrid>& nodes) {
        for (int i = 0; i < nodes.size(); ++i) {
            if (nodes[i].id == id) {
                nodes.removeAt(i);
                return true;
            }
            if (erase(nodes[i].subGrids)) {
                return true;
            }
        }
        return false;
    };
    erase(doc.grids);
}

} // namespace PageEdit
} // namespace gazer
