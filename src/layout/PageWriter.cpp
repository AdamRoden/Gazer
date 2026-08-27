#include "layout/PageWriter.h"

#include "layout/PageActionParse.h"
#include "layout/PageDim.h"

#include <QFile>
#include <QStringList>
#include <QXmlStreamWriter>

namespace gazer {

namespace {

QString colorTok(const std::optional<QColor>& c)
{
    if (!c || !c->isValid()) {
        return {};
    }
    if (c->alpha() < 255) {
        return c->name(QColor::HexArgb);
    }
    return c->name(QColor::HexRgb);
}

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

void writeChrome(QXmlStreamWriter& xml, const PageChrome& st)
{
    attr(xml, QStringLiteral("background"), colorTok(st.background));
    attr(xml, QStringLiteral("foreground"), colorTok(st.foreground));
    attr(xml, QStringLiteral("border"), colorTok(st.borderColor));
    if (st.thickness && st.thickness->isSet()) {
        xml.writeAttribute(QStringLiteral("thickness"), st.thickness->toToken());
    }
    if (st.radius && st.radius->isSet()) {
        xml.writeAttribute(QStringLiteral("radius"), st.radius->toToken());
    }
    if (st.blur) {
        xml.writeAttribute(QStringLiteral("blur"), QString::number(*st.blur, 'g', 8));
    }
    if (st.progressStyle) {
        const QString csv = st.progressStyle->toCsv();
        if (!csv.isEmpty()) {
            xml.writeAttribute(QStringLiteral("progressStyle"), csv);
        }
    }
    attr(xml, QStringLiteral("progressColor"), colorTok(st.progressColor));
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

void writeLeafAttrs(QXmlStreamWriter& xml, const PageLeaf& leaf)
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
    attr(xml, QStringLiteral("cluster"), leaf.cluster);
    attr(xml, QStringLiteral("clusterSlot"), leaf.clusterSlot);
    attr(xml, QStringLiteral("textStyle"), leaf.textStyle);
    attr(xml, QStringLiteral("visibleWhen"), leaf.visibleWhen);
    attrBool(xml, QStringLiteral("interactive"), leaf.interactive, true);
    attrBool(xml, QStringLiteral("suspendExempt"), leaf.suspendExempt, false);
    attrBool(xml, QStringLiteral("actionLoop"), leaf.actionLoop, false);
    attrBool(xml, QStringLiteral("show"), leaf.show, true);
    attrBool(xml, QStringLiteral("shell"), leaf.shell, false);
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
    if (a.type == PageActionType::GoBack && value.isEmpty()) {
        value = QStringLiteral("true");
    }
    attr(xml, name, value);
    return true;
}

void writeLeafBody(QXmlStreamWriter& xml, const PageLeaf& leaf, bool inlined)
{
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
    if (!grid.rowWeights.isEmpty()) {
        bool nontrivial = grid.rowWeights.size() != grid.rows;
        QStringList parts;
        parts.reserve(grid.rowWeights.size());
        for (double w : grid.rowWeights) {
            if (!qFuzzyCompare(w + 1.0, 2.0)) {
                nontrivial = true;
            }
            parts.push_back(QString::number(w, 'g', 8));
        }
        if (nontrivial) {
            xml.writeAttribute(QStringLiteral("rowWeights"), parts.join(QLatin1Char(',')));
        }
    }
    attrBool(xml, QStringLiteral("drawerMotion"), grid.drawerMotion, false);
    if (grid.rootSlot == PageRootSlot::Drawer) {
        xml.writeAttribute(QStringLiteral("chrome"), QStringLiteral("drawer"));
    } else if (grid.rootSlot == PageRootSlot::Quit) {
        xml.writeAttribute(QStringLiteral("chrome"), QStringLiteral("quit"));
    }
    attrBool(xml, QStringLiteral("autoClose"), grid.autoClose, false);
    attrInt(xml, QStringLiteral("autoCloseIdleMs"), grid.autoCloseIdleMs, -1);
    attrBool(xml, QStringLiteral("show"), grid.show, true);
    attrBool(xml, QStringLiteral("shell"), grid.shell, false);
    attr(xml, QStringLiteral("style"), grid.styleId);
    attr(xml, QStringLiteral("dwell"), grid.dwellId);
    writeChrome(xml, grid.style);
    writeDwell(xml, grid.dwell);
    for (const PageCell& cell : grid.cells) {
        xml.writeStartElement(QStringLiteral("Cell"));
        writeLeafAttrs(xml, cell);
        attrInt(xml, QStringLiteral("row"), cell.row, 0);
        attrInt(xml, QStringLiteral("col"), cell.col, 0);
        attrInt(xml, QStringLiteral("rowSpan"), cell.rowSpan, 1);
        attrInt(xml, QStringLiteral("colSpan"), cell.colSpan, 1);
        const bool inlined = writeInlineAction(xml, cell.actions);
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
    attrInt(xml, QStringLiteral("autoCloseIdleMs"), doc.autoCloseIdleMs, -1);
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
        writeLeafAttrs(xml, z);
        writePlacement(xml, z.desktopMode, z.anchor, z.offset, z.size);
        attr(xml, QStringLiteral("dwellOffset"), pairTok(z.dwellOffset));
        attr(xml, QStringLiteral("dwellSize"), pairTok(z.dwellSize));
        const bool inlined = writeInlineAction(xml, z.actions);
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
