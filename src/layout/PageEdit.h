#pragma once

#include "layout/PageTypes.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <functional>

namespace gazer {
namespace PageEdit {

[[nodiscard]] PageGrid* primaryGrid(PageDocument& doc);
[[nodiscard]] const PageGrid* primaryGrid(const PageDocument& doc);
[[nodiscard]] PageGrid* findGrid(PageDocument& doc, const QString& id);
[[nodiscard]] const PageGrid* findGrid(const PageDocument& doc, const QString& id);
[[nodiscard]] PageCell* findCell(PageDocument& doc, const QString& id);
[[nodiscard]] const PageCell* findCell(const PageDocument& doc, const QString& id);
[[nodiscard]] PageZone* findZone(PageDocument& doc, const QString& id);
[[nodiscard]] const PageZone* findZone(const PageDocument& doc, const QString& id);
[[nodiscard]] PageLeaf* findLeaf(PageDocument& doc, const QString& id);
[[nodiscard]] const PageLeaf* findLeaf(const PageDocument& doc, const QString& id);
[[nodiscard]] bool isZone(const PageDocument& doc, const QString& id);
[[nodiscard]] QStringList allIds(const PageDocument& doc);
[[nodiscard]] QVector<int> usedLayers(const PageDocument& doc);
/// First ShowLayers action: zones, then cells. Null if none.
[[nodiscard]] const PageAction* firstShowLayers(const PageDocument& doc);

void forEachGrid(PageDocument& doc, const std::function<void(PageGrid&)>& fn);
void forEachGrid(const PageDocument& doc, const std::function<void(const PageGrid&)>& fn);
void forEachCell(PageDocument& doc, const std::function<void(PageGrid&, PageCell&)>& fn);

void ensurePrimaryGrid(PageDocument& doc);
void ensureGridFits(PageDocument& doc);
void expandForCell(PageGrid& grid, const PageCell& cell);
[[nodiscard]] bool findEmptyCell(const PageDocument& doc, int& row, int& col);
[[nodiscard]] bool findEmptyCell(const PageGrid& grid, int& row, int& col);
[[nodiscard]] PageGrid* gridOwningCell(PageDocument& doc, const QString& cellId);
[[nodiscard]] const PageGrid* gridOwningCell(const PageDocument& doc, const QString& cellId);
void remapPageActionTargets(PageDocument& doc, const QHash<QString, QString>& idMap);
void remapStyleId(PageDocument& doc, const QString& from, const QString& to);
void remapDwellId(PageDocument& doc, const QString& from, const QString& to);
[[nodiscard]] PageZone zoneFromCell(const PageCell& cell);
[[nodiscard]] PageCell cellFromZone(const PageZone& zone);
void removeLeaf(PageDocument& doc, const QString& id);
void removeGrid(PageDocument& doc, const QString& id);

} // namespace PageEdit
} // namespace gazer
