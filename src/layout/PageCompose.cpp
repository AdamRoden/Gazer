#include "layout/PageCompose.h"

#include "layout/PageEdit.h"

#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace gazer {
namespace PageCompose {

void forEachSrc(const PageDocument& doc, const std::function<void(const PageGrid&)>& fn)
{
    PageEdit::forEachGrid(doc, [&](const PageGrid& g) {
        if (!g.src.isEmpty()) {
            fn(g);
        }
    });
}

void collectSlotTargets(const PageDocument& doc, QSet<QString>& ids)
{
    auto noteActions = [&](const QVector<PageAction>& actions) {
        for (const PageAction& a : actions) {
            if (a.type == PageActionType::HostPage && !a.targetId.isEmpty()) {
                ids.insert(a.targetId);
            }
        }
    };
    PageEdit::forEachGrid(doc, [&](const PageGrid& g) {
        if (!g.src.isEmpty()) {
            ids.insert(g.src);
        }
        for (const PageCell& c : g.cells) {
            noteActions(c.actions);
            for (const PagePhase& p : c.phases) {
                noteActions(p.actions);
            }
        }
    });
    for (const PageZone& z : doc.zones) {
        noteActions(z.actions);
        for (const PagePhase& p : z.phases) {
            noteActions(p.actions);
        }
    }
}

const PageGrid* findSrcSlot(const PageDocument& doc)
{
    const PageGrid* slot = nullptr;
    PageEdit::forEachGrid(doc, [&](const PageGrid& g) {
        if (!slot && !g.src.isEmpty()) {
            slot = &g;
        }
    });
    return slot;
}

PageGrid* findSrcSlot(PageDocument& doc)
{
    PageGrid* slot = nullptr;
    PageEdit::forEachGrid(doc, [&](PageGrid& g) {
        if (!slot && !g.src.isEmpty()) {
            slot = &g;
        }
    });
    return slot;
}

QString firstSrc(const PageDocument& doc)
{
    const PageGrid* slot = findSrcSlot(doc);
    return slot ? slot->src : QString();
}

const PageGrid* sourcedGrid(const PageDocument& fragment, const QString& srcGrid, QString* error)
{
    if (srcGrid.trimmed().isEmpty()) {
        if (fragment.grids.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Page '%1' has no grid to host").arg(fragment.id);
            }
            return nullptr;
        }
        return &fragment.grids.first();
    }
    const PageGrid* g = fragment.findGrid(srcGrid.trimmed());
    if (!g) {
        if (error) {
            *error = QStringLiteral("Grid '%1' not found in page '%2'").arg(srcGrid, fragment.id);
        }
        return nullptr;
    }
    return g;
}

bool resolveAll(const PageDocument& root, const LoadFn& load, QHash<QString, PageDocument>& out,
                QString* error)
{
    QSet<QString> walking;
    std::function<bool(const QString&, const QString&)> loadInto =
        [&](const QString& id, const QString& srcGrid) -> bool {
        if (const auto it = out.constFind(id); it != out.cend()) {
            return sourcedGrid(it.value(), srcGrid, error) != nullptr;
        }
        if (walking.contains(id)) {
            if (error) {
                *error = QStringLiteral("src cycle involving '%1'").arg(id);
            }
            return false;
        }
        walking.insert(id);
        PageDocument frag;
        if (!load(id, frag, error)) {
            walking.remove(id);
            return false;
        }
        if (frag.id != id) {
            if (error) {
                *error = QStringLiteral("Page id '%1' does not match src '%2'").arg(frag.id, id);
            }
            walking.remove(id);
            return false;
        }
        if (!sourcedGrid(frag, srcGrid, error)) {
            walking.remove(id);
            return false;
        }
        QVector<QPair<QString, QString>> children;
        forEachSrc(frag, [&](const PageGrid& g) { children.push_back({g.src, g.srcGrid}); });
        for (const auto& child : children) {
            if (!loadInto(child.first, child.second)) {
                walking.remove(id);
                return false;
            }
        }
        out.insert(id, std::move(frag));
        walking.remove(id);
        return true;
    };

    bool ok = true;
    forEachSrc(root, [&](const PageGrid& g) {
        if (ok && !loadInto(g.src, g.srcGrid)) {
            ok = false;
        }
    });
    return ok;
}

QHash<QString, const PageDocument*> pointers(const QHash<QString, PageDocument>& docs)
{
    QHash<QString, const PageDocument*> out;
    for (auto it = docs.cbegin(); it != docs.cend(); ++it) {
        out.insert(it.key(), &it.value());
    }
    return out;
}

} // namespace PageCompose
} // namespace gazer
