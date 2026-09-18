#include "layout/PageWriter.h"

#include "layout/PageActionParse.h"
#include "layout/PageDim.h"

#include <QFile>
#include <QStringList>
#include <QXmlStreamWriter>

namespace gazer {

namespace {

QString pairTok(const PageDimPair& p)
{
    if (!p.isSet()) {
        return {};
    }
    const QString x = p.x.isSet() ? PageDimParse::token(p.x) : QStringLiteral("0");
    const QString y = p.y.isSet() ? PageDimParse::token(p.y) : QStringLiteral("0");
    return x + QLatin1Char(',') + y;
}

void attr(QXmlStreamWriter& xml, const QString& name, const QString& value)
{
    if (!value.isEmpty()) {
        xml.writeAttribute(name, value);
    }
}

void attrBool(QXmlStreamWriter& xml, const QString& name, bool value, bool defaultValue)
{
    if (value != defaultValue) {
        xml.writeAttribute(name, value ? QStringLiteral("true") : QStringLiteral("false"));
    }
}

void attrInt(QXmlStreamWriter& xml, const QString& name, int value, int defaultValue)
{
    if (value != defaultValue) {
        xml.writeAttribute(name, QString::number(value));
    }
}

void attrLayers(QXmlStreamWriter& xml, const QString& name, const QVector<int>& layers)
{
    if (!isDefaultLayerList(layers)) {
        xml.writeAttribute(name, layerListCsv(layers));
    }
}

void writeChrome(QXmlStreamWriter& xml, const PageChrome& st, bool includeItemPaint = true)
{
    attr(xml, QStringLiteral("background"), st.background.token);
    if (includeItemPaint) {
        attr(xml, QStringLiteral("foreground"), st.foreground.token);
    }
    attr(xml, QStringLiteral("border"), st.borderColor.token);
    if (st.thickness && st.thickness->isSet()) {
        xml.writeAttribute(QStringLiteral("thickness"), st.thickness->toToken());
    }
    if (st.radius && st.radius->isSet()) {
        xml.writeAttribute(QStringLiteral("radius"), st.radius->toToken());
    }
    if (st.blur) {
        xml.writeAttribute(QStringLiteral("blur"), QString::number(*st.blur, 'g', 8));
    }
    if (!includeItemPaint) {
        return;
    }
    if (st.progressStyle) {
        const QString csv = st.progressStyle->toCsv();
        if (!csv.isEmpty()) {
            xml.writeAttribute(QStringLiteral("progressStyle"), csv);
        }
    }
    attr(xml, QStringLiteral("progressColor"), st.progressColor.token);
}

void writeDwell(QXmlStreamWriter& xml, const PageDwell& d)
{
    if (d.scanGrace) {
        xml.writeAttribute(QStringLiteral("scanGrace"), QString::number(*d.scanGrace));
    }
    if (d.dwellGrace) {
        xml.writeAttribute(QStringLiteral("dwellGrace"), QString::number(*d.dwellGrace));
    }
    if (d.activation) {
        QStringList parts;
        for (int n : *d.activation) {
            parts.push_back(QString::number(n));
        }
        xml.writeAttribute(QStringLiteral("activation"), parts.join(QLatin1Char(',')));
    }
}

void writePlacement(QXmlStreamWriter& xml, bool desktopMode, PageAnchor anchor,
                    const PageDimPair& offset, const PageDimPair& size)
{
    attrBool(xml, QStringLiteral("desktopMode"), desktopMode, false);
    attr(xml, QStringLiteral("anchor"), PageDimParse::anchorName(anchor));
    attr(xml, QStringLiteral("offset"), pairTok(offset));
    attr(xml, QStringLiteral("size"), pairTok(size));
}

void writeLeafAttrs(QXmlStreamWriter& xml, const PageLeaf& leaf, bool writeShell)
{
    attr(xml, QStringLiteral("id"), leaf.id);
    attr(xml, QStringLiteral("style"), leaf.styleId);
    attr(xml, QStringLiteral("dwell"), leaf.dwellId);
    attr(xml, QStringLiteral("label"), leaf.label);
    attr(xml, QStringLiteral("icon"), leaf.icon);
    attr(xml, QStringLiteral("caption"), leaf.caption);
    attr(xml, QStringLiteral("settingKey"), leaf.settingKey);
    attr(xml, QStringLiteral("activeState"), leaf.activeState);
    attr(xml, QStringLiteral("role"), leaf.role);
    attr(xml, QStringLiteral("textStyle"), leaf.textStyle);
    attr(xml, QStringLiteral("visibleWhen"), leaf.visibleWhen);
    attrBool(xml, QStringLiteral("suspendExempt"), leaf.suspendExempt, false);
    attrBool(xml, QStringLiteral("actionLoop"), leaf.actionLoop, false);
    if (writeShell) {
        attrBool(xml, QStringLiteral("shell"), leaf.shell, false);
    }
    writeChrome(xml, leaf.style);
    writeDwell(xml, leaf.dwell);
}

void writeAction(QXmlStreamWriter& xml, const PageAction& a)
{
    if (a.type == PageActionType::Ahk) {
        xml.writeStartElement(QStringLiteral("AHK"));
        if (a.ahkSource.contains(QLatin1String("]]>"))) {
            xml.writeCharacters(a.ahkSource);
        } else {
            xml.writeCDATA(a.ahkSource);
        }
        xml.writeEndElement();
        return;
    }
    xml.writeStartElement(pageActionElementName(a));
    const QString value = pageActionValueText(a);
    if (a.type != PageActionType::GoBack || !value.isEmpty()) {
        attr(xml, QStringLiteral("value"), value);
    }
    attr(xml, QStringLiteral("args"), a.args);
    xml.writeEndElement();
}

bool writeInlineAction(QXmlStreamWriter& xml, const QVector<PageAction>& acts)
{
    if (acts.size() != 1 || !pageActionCanInline(acts[0])) {
        return false;
    }
    const PageAction& a = acts[0];
    const QString name = pageActionAttributeName(a);
    QString value = pageActionValueText(a);
    if ((a.type == PageActionType::GoBack
         || (a.type == PageActionType::Nav && a.verb == PageVerb::Close
             && a.targetScope != PageNavScope::Id))
        && value.isEmpty()) {
        value = QStringLiteral("true");
    }
    xml.writeAttribute(name, value);
    return true;
}

void writeLeafBody(QXmlStreamWriter& xml, const PageLeaf& leaf, bool inlined)
{
    if (!leaf.phases.isEmpty()) {
        for (const PagePhase& phase : leaf.phases) {
            xml.writeStartElement(QStringLiteral("Phase"));
            if (!writeInlineAction(xml, phase.actions)) {
                for (const PageAction& a : phase.actions) {
                    writeAction(xml, a);
                }
            }
            xml.writeEndElement();
        }
        return;
    }
    if (inlined) {
        return;
    }
    for (const PageAction& a : leaf.actions) {
        writeAction(xml, a);
    }
}

void writeGrid(QXmlStreamWriter& xml, const PageGrid& grid)
{
    xml.writeStartElement(grid.nested ? QStringLiteral("SubGrid") : QStringLiteral("Grid"));
    attr(xml, QStringLiteral("id"), grid.id);
    attrInt(xml, QStringLiteral("rows"), grid.rows, 1);
    attrInt(xml, QStringLiteral("columns"), grid.columns, 1);
    if (grid.nested) {
        attrInt(xml, QStringLiteral("row"), grid.row, 0);
        attrInt(xml, QStringLiteral("col"), grid.col, 0);
        attrInt(xml, QStringLiteral("rowSpan"), grid.rowSpan, 1);
        attrInt(xml, QStringLiteral("colSpan"), grid.colSpan, 1);
    } else {
        writePlacement(xml, grid.desktopMode, grid.anchor, grid.offset, grid.size);
    }
    attrInt(xml, QStringLiteral("gap"), grid.gapPx, 0);
    attrInt(xml, QStringLiteral("margin"), grid.marginPx, 0);
    auto omitEqualStars = [](const QVector<PageTrackSize>& tracks, int count) {
        if (tracks.isEmpty()) {
            return true;
        }
        if (tracks.size() != count) {
            return false;
        }
        for (const PageTrackSize& t : tracks) {
            if (!t.isUnitStar()) {
                return false;
            }
        }
        return true;
    };
    if (!omitEqualStars(grid.rowTracks, grid.rows)) {
        xml.writeAttribute(QStringLiteral("rowHeights"), PageDimParse::tokenList(grid.rowTracks));
    }
    if (!omitEqualStars(grid.columnTracks, grid.columns)) {
        xml.writeAttribute(QStringLiteral("columnWidths"),
                           PageDimParse::tokenList(grid.columnTracks));
    }
    attrBool(xml, QStringLiteral("drawerMotion"), grid.drawerMotion, false);
    attrBool(xml, QStringLiteral("autoClose"), grid.autoClose, false);
    attrLayers(xml, QStringLiteral("layers"), grid.layers);
    attrBool(xml, QStringLiteral("shell"), grid.shell, false);
    attr(xml, QStringLiteral("style"), grid.styleId);
    attr(xml, QStringLiteral("dwell"), grid.dwellId);
    attr(xml, QStringLiteral("src"), grid.src);
    attr(xml, QStringLiteral("grid"), grid.srcGrid);
    writeChrome(xml, grid.style, false);
    writeDwell(xml, grid.dwell);
    if (!grid.src.isEmpty()) {
        xml.writeEndElement();
        return;
    }
    for (const PageCell& cell : grid.cells) {
        xml.writeStartElement(QStringLiteral("Cell"));
        writeLeafAttrs(xml, cell, false);
        attrInt(xml, QStringLiteral("row"), cell.row, 0);
        attrInt(xml, QStringLiteral("col"), cell.col, 0);
        attrInt(xml, QStringLiteral("rowSpan"), cell.rowSpan, 1);
        attrInt(xml, QStringLiteral("colSpan"), cell.colSpan, 1);
        const bool inlined = cell.phases.isEmpty() && writeInlineAction(xml, cell.actions);
        writeLeafBody(xml, cell, inlined);
        xml.writeEndElement();
    }
    for (const PageGrid& sub : grid.subGrids) {
        writeGrid(xml, sub);
    }
    xml.writeEndElement();
}

} // namespace

QByteArray PageWriter::toBytes(const PageDocument& doc)
{
    QByteArray out;
    QXmlStreamWriter xml(&out);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("Page"));
    attr(xml, QStringLiteral("id"), doc.id);
    attr(xml, QStringLiteral("name"), doc.name);
    attrBool(xml, QStringLiteral("master"), doc.master, false);
    attrBool(xml, QStringLiteral("autoClose"), doc.autoClose, false);
    attrLayers(xml, QStringLiteral("showLayers"), doc.showLayers);
    writeChrome(xml, doc.style);
    writeDwell(xml, doc.dwell);
    for (auto it = doc.styles.cbegin(); it != doc.styles.cend(); ++it) {
        xml.writeStartElement(QStringLiteral("Style"));
        xml.writeAttribute(QStringLiteral("id"), it.key());
        writeChrome(xml, it.value());
        xml.writeEndElement();
    }
    for (auto it = doc.dwells.cbegin(); it != doc.dwells.cend(); ++it) {
        xml.writeStartElement(QStringLiteral("Dwell"));
        xml.writeAttribute(QStringLiteral("id"), it.key());
        writeDwell(xml, it.value());
        xml.writeEndElement();
    }
    for (const PageZone& z : doc.zones) {
        xml.writeStartElement(QStringLiteral("Zone"));
        writeLeafAttrs(xml, z, true);
        attrLayers(xml, QStringLiteral("layers"), z.layers);
        writePlacement(xml, z.desktopMode, z.anchor, z.offset, z.size);
        attr(xml, QStringLiteral("dwellOffset"), pairTok(z.dwellOffset));
        attr(xml, QStringLiteral("dwellSize"), pairTok(z.dwellSize));
        const bool inlined = z.phases.isEmpty() && writeInlineAction(xml, z.actions);
        writeLeafBody(xml, z, inlined);
        xml.writeEndElement();
    }
    for (const PageGrid& g : doc.grids) {
        writeGrid(xml, g);
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    return out;
}

bool PageWriter::saveToFile(const PageDocument& doc, const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(path);
        }
        return false;
    }
    const QByteArray bytes = toBytes(doc);
    if (f.write(bytes) != bytes.size()) {
        if (error) {
            *error = QStringLiteral("Incomplete write to %1").arg(path);
        }
        return false;
    }
    return true;
}

} // namespace gazer
