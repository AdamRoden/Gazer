#include "layout/LayoutWriter.h"

#include "layout/LayoutSchema.h"
#include "ui/Theme.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace gazer {

namespace {

QJsonObject actionToJson(const LayoutAction& a)
{
    QJsonObject o;
    const QString type = LayoutSchema::actionTypeName(a.type);
    if (!type.isEmpty()) {
        o.insert(QStringLiteral("type"), type);
    }
    if (!a.text.isEmpty()) {
        o.insert(QStringLiteral("text"), a.text);
    }
    if (!a.layoutId.isEmpty()) {
        o.insert(QStringLiteral("layoutId"), a.layoutId);
    }
    if (!a.name.isEmpty()) {
        o.insert(QStringLiteral("name"), a.name);
    }
    if (!a.source.isEmpty()) {
        o.insert(QStringLiteral("source"), a.source);
    }
    if (a.delayMs != 0) {
        o.insert(QStringLiteral("delayMs"), a.delayMs);
    }
    return o;
}

QJsonArray actionListToJson(const QVector<LayoutAction>& acts)
{
    QJsonArray arr;
    for (const LayoutAction& a : acts) {
        if (a.type == LayoutAction::Type::Unknown) {
            continue;
        }
        arr.append(actionToJson(a));
    }
    return arr;
}

void writeDim(QJsonObject& o, const QString& key, const DimSpec& d, bool percentAsString = false)
{
    if (!d.isSet()) {
        return;
    }
    if (d.unit == DimSpec::Unit::Pixels) {
        o.insert(key + QStringLiteral("Px"), d.value);
    } else if (percentAsString) {
        o.insert(key, QString::number(d.value) + QLatin1Char('%'));
    } else {
        o.insert(key, d.value);
    }
}

QJsonObject chromeToJson(const LayoutChromeStyle& st)
{
    QJsonObject o;
    if (st.background) {
        o.insert(QStringLiteral("background"), ThemeColors::colorToHex(*st.background));
    }
    if (st.foreground) {
        o.insert(QStringLiteral("foreground"), ThemeColors::colorToHex(*st.foreground));
    }
    if (st.borderColor) {
        o.insert(QStringLiteral("borderColor"), ThemeColors::colorToHex(*st.borderColor));
    }
    if (st.borderWidth) {
        o.insert(QStringLiteral("borderWidth"), *st.borderWidth);
    }
    if (st.radius) {
        o.insert(QStringLiteral("radius"), *st.radius);
    }
    if (st.blur) {
        o.insert(QStringLiteral("blur"), *st.blur);
    }
    return o;
}

QJsonObject dwellToJson(const LayoutDwellConfig& d)
{
    QJsonObject o;
    o.insert(QStringLiteral("enabled"), d.enabled);
    if (d.hasTiming || !d.msSequence.isEmpty() || d.ms != 800) {
        if (d.msSequence.size() > 1) {
            QJsonArray seq;
            for (int ms : d.msSequence) {
                seq.append(ms);
            }
            o.insert(QStringLiteral("ms"), seq);
        } else {
            o.insert(QStringLiteral("ms"), d.ms);
        }
    }
    if (d.hasProgressStyle || d.progressStyle != QLatin1String("radial")) {
        o.insert(QStringLiteral("progressStyle"), d.progressStyle);
    }
    if (d.hasGrace && d.graceMs >= 0) {
        o.insert(QStringLiteral("graceMs"), d.graceMs);
    }
    if (d.hasScanGrace && d.scanGraceMs >= 0) {
        o.insert(QStringLiteral("scanGraceMs"), d.scanGraceMs);
    }
    if (d.hasProgressColor && !d.progressColor.isEmpty()) {
        o.insert(QStringLiteral("progressColor"), d.progressColor);
    }
    if (d.hasFillColor && !d.fillColor.isEmpty()) {
        o.insert(QStringLiteral("fillColor"), d.fillColor);
    }
    if (d.hasBorderColor && !d.borderColor.isEmpty()) {
        o.insert(QStringLiteral("borderColor"), d.borderColor);
    }
    if (d.hasFlashColor && !d.flashColor.isEmpty()) {
        o.insert(QStringLiteral("flashColor"), d.flashColor);
    }
    if (d.hasFlashMs && d.flashMs >= 0) {
        o.insert(QStringLiteral("flashMs"), d.flashMs);
    }
    return o;
}

QJsonObject windowToJson(const LayoutWindowPlacement& p)
{
    QJsonObject o;
    if (p.hidden) {
        o.insert(QStringLiteral("hidden"), true);
    }
    o.insert(QStringLiteral("anchor"), LayoutSchema::windowAnchorName(p.anchor));
    writeDim(o, QStringLiteral("width"), p.width);
    writeDim(o, QStringLiteral("height"), p.height);
    writeDim(o, QStringLiteral("x"), p.x);
    writeDim(o, QStringLiteral("y"), p.y);
    if (p.marginPx != 0) {
        o.insert(QStringLiteral("marginPx"), p.marginPx);
    }
    if (p.hasBoundsMode) {
        o.insert(QStringLiteral("boundsMode"), LayoutSchema::boundsModeName(p.boundsMode));
    }
    if (p.style.hasAny()) {
        o.insert(QStringLiteral("style"), chromeToJson(p.style));
    }
    if (p.aboveTaskbar) {
        o.insert(QStringLiteral("aboveTaskbar"), true);
    }
    if (p.drawerMotion) {
        o.insert(QStringLiteral("drawerMotion"), true);
    }
    return o;
}

QJsonObject dwellRegionToJson(const LayoutDwellRegion& r)
{
    QJsonObject o;
    const QString sa = LayoutSchema::screenAnchorName(r.screenAnchor);
    if (!sa.isEmpty()) {
        o.insert(QStringLiteral("screenAnchor"), sa);
    }
    writeDim(o, QStringLiteral("x"), r.x, true);
    writeDim(o, QStringLiteral("y"), r.y, true);
    writeDim(o, QStringLiteral("width"), r.width, true);
    writeDim(o, QStringLiteral("height"), r.height, true);
    if (r.marginPx != 4) {
        o.insert(QStringLiteral("marginPx"), r.marginPx);
    }
    if (r.hasBoundsMode) {
        o.insert(QStringLiteral("boundsMode"), LayoutSchema::boundsModeName(r.boundsMode));
    }
    return o;
}

QJsonObject itemToJson(const LayoutItem& item)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), item.id);
    if (!item.label.isEmpty()) {
        o.insert(QStringLiteral("label"), item.label);
    }
    if (!item.caption.isEmpty()) {
        o.insert(QStringLiteral("caption"), item.caption);
    }
    if (!item.settingKey.isEmpty()) {
        o.insert(QStringLiteral("settingKey"), item.settingKey);
    }
    if (!item.activeState.isEmpty()) {
        o.insert(QStringLiteral("activeState"), item.activeState);
    }
    if (!item.icon.isEmpty()) {
        o.insert(QStringLiteral("icon"), item.icon);
    }
    if (!item.role.isEmpty()) {
        o.insert(QStringLiteral("role"), item.role);
    }
    if (!item.textStyle.isEmpty()) {
        o.insert(QStringLiteral("textStyle"), item.textStyle);
    }
    if (!item.cluster.isEmpty()) {
        o.insert(QStringLiteral("cluster"), item.cluster);
    }
    if (!item.clusterSlot.isEmpty()) {
        o.insert(QStringLiteral("clusterSlot"), item.clusterSlot);
    }
    if (!item.interactive) {
        o.insert(QStringLiteral("interactive"), false);
    }
    if (item.dwellExempt) {
        o.insert(QStringLiteral("dwellExempt"), true);
    }
    o.insert(QStringLiteral("row"), item.row);
    o.insert(QStringLiteral("col"), item.col);
    if (item.rowSpan != 1) {
        o.insert(QStringLiteral("rowSpan"), item.rowSpan);
    }
    if (item.colSpan != 1) {
        o.insert(QStringLiteral("colSpan"), item.colSpan);
    }
    if (item.widthUnits > 0.0) {
        o.insert(QStringLiteral("u"), item.widthUnits);
    }
    if (item.actionLoop) {
        o.insert(QStringLiteral("actionLoop"), true);
    }
    if (item.unbounded) {
        o.insert(QStringLiteral("unbounded"), true);
    }
    if (!item.visible) {
        o.insert(QStringLiteral("visible"), false);
    }
    if (!item.visibleWhen.isEmpty()) {
        o.insert(QStringLiteral("visibleWhen"), item.visibleWhen);
    }
    if (!item.type.isEmpty()) {
        o.insert(QStringLiteral("type"), item.type);
    }
    if (!item.embedLayoutId.isEmpty()) {
        o.insert(QStringLiteral("layoutId"), item.embedLayoutId);
    }
    if (item.hasDwellRegion) {
        o.insert(QStringLiteral("dwellRegion"), dwellRegionToJson(item.dwellRegion));
    }
    if (item.dwell.sectionPresent) {
        o.insert(QStringLiteral("dwell"), dwellToJson(item.dwell));
    }
    if (item.style.hasAny()) {
        o.insert(QStringLiteral("style"), chromeToJson(item.style));
    }

    const QVector<LayoutAction> acts = item.effectiveActions();
    if (acts.size() == 1) {
        o.insert(QStringLiteral("action"), actionToJson(acts.first()));
    } else if (acts.size() > 1) {
        o.insert(QStringLiteral("actions"), actionListToJson(acts));
    }
    return o;
}

} // namespace

QJsonObject LayoutWriter::toJson(const LayoutDocument& doc)
{
    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), doc.schemaVersion);
    o.insert(QStringLiteral("id"), doc.id);
    if (!doc.name.isEmpty()) {
        o.insert(QStringLiteral("name"), doc.name);
    }
    if (!doc.description.isEmpty()) {
        o.insert(QStringLiteral("description"), doc.description);
    }
    if (doc.master) {
        o.insert(QStringLiteral("master"), true);
    }
    if (doc.hideUntilGazeReveal) {
        o.insert(QStringLiteral("hideUntilGazeReveal"), true);
    }
    if (!doc.children.isEmpty()) {
        QJsonArray kids;
        for (const LayoutChildRef& c : doc.children) {
            QJsonObject ko;
            ko.insert(QStringLiteral("id"), c.id);
            ko.insert(QStringLiteral("layoutId"), c.layoutId);
            if (!c.visible) {
                ko.insert(QStringLiteral("visible"), false);
            }
            if (!c.visibleWhen.isEmpty()) {
                ko.insert(QStringLiteral("visibleWhen"), c.visibleWhen);
            }
            kids.append(ko);
        }
        o.insert(QStringLiteral("children"), kids);
    }
    if (doc.hasBoundsMode) {
        o.insert(QStringLiteral("boundsMode"), LayoutSchema::boundsModeName(doc.boundsMode));
    }
    if (doc.master) {
        o.insert(QStringLiteral("autoClose"), doc.autoClose);
    } else if (!doc.autoClose) {
        o.insert(QStringLiteral("autoClose"), false);
    }
    if (doc.autoCloseIdleMs >= 0) {
        o.insert(QStringLiteral("autoCloseIdleMs"), doc.autoCloseIdleMs);
    }
    if (doc.autoCloseFadeMs >= 0) {
        o.insert(QStringLiteral("autoCloseFadeMs"), doc.autoCloseFadeMs);
    }

    QJsonObject grid;
    grid.insert(QStringLiteral("columns"), doc.grid.columns);
    grid.insert(QStringLiteral("rows"), doc.grid.rows);
    grid.insert(QStringLiteral("gapPx"), doc.grid.gapPx);
    grid.insert(QStringLiteral("marginPx"), doc.grid.marginPx);
    writeDim(grid, QStringLiteral("marginX"), doc.grid.marginX);
    writeDim(grid, QStringLiteral("marginY"), doc.grid.marginY);
    if (doc.grid.unitRows) {
        grid.insert(QStringLiteral("unitRows"), true);
    }
    o.insert(QStringLiteral("grid"), grid);

    if (doc.dwell.sectionPresent) {
        o.insert(QStringLiteral("dwell"), dwellToJson(doc.dwell));
    }
    if (doc.placement.specified || doc.placement.hidden) {
        o.insert(QStringLiteral("window"), windowToJson(doc.placement));
    }
    if (doc.style.hasAny()) {
        o.insert(QStringLiteral("style"), chromeToJson(doc.style));
    }
    if (!doc.onOpen.isEmpty()) {
        o.insert(QStringLiteral("onOpen"), actionListToJson(doc.onOpen));
    }
    if (!doc.onLoad.isEmpty()) {
        o.insert(QStringLiteral("onLoad"), actionListToJson(doc.onLoad));
    }
    if (!doc.onClose.isEmpty()) {
        o.insert(QStringLiteral("onClose"), actionListToJson(doc.onClose));
    }

    QJsonArray items;
    for (const LayoutItem& item : doc.items) {
        items.append(itemToJson(item));
    }
    o.insert(QStringLiteral("items"), items);
    return o;
}

QByteArray LayoutWriter::toBytes(const LayoutDocument& doc)
{
    return QJsonDocument(toJson(doc)).toJson(QJsonDocument::Indented);
}

bool LayoutWriter::saveToFile(const LayoutDocument& doc, const QString& path, QString* error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot write layout: %1").arg(path);
        }
        return false;
    }
    const QByteArray bytes = toBytes(doc);
    if (f.write(bytes) != bytes.size()) {
        if (error) {
            *error = QStringLiteral("Incomplete write: %1").arg(path);
        }
        return false;
    }
    if (!f.commit()) {
        if (error) {
            *error = QStringLiteral("Failed to commit: %1").arg(path);
        }
        return false;
    }
    return true;
}

} // namespace gazer
