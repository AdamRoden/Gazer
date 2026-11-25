#include "layout/PageLoader.h"

#include "layout/PageActionParse.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageXml.h"

#include <QFile>
#include <QXmlStreamReader>
#include <utility>

namespace gazer {

namespace {

QString xmlError(QXmlStreamReader& xml, const QString& msg)
{
    return QStringLiteral("%1 (line %2)").arg(msg).arg(xml.lineNumber());
}

bool parseBoolAttr(const QStringView v, bool defaultValue, bool* out, QString* error,
                   const QString& name)
{
    const QString s = v.toString().trimmed();
    if (s.isEmpty()) {
        if (out) {
            *out = defaultValue;
        }
        return true;
    }
    bool flag = false;
    if (!PageDimParse::strictBool(v, &flag)) {
        if (error) {
            *error = QStringLiteral("Expected true/false for %1, got '%2'").arg(name, s);
        }
        return false;
    }
    if (out) {
        *out = flag;
    }
    return true;
}

bool parseIntAttr(const QStringView v, int defaultValue, int* out, QString* error,
                  const QString& name)
{
    const QString s = v.toString().trimmed();
    if (s.isEmpty()) {
        if (out) {
            *out = defaultValue;
        }
        return true;
    }
    bool ok = false;
    const int n = s.toInt(&ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("Invalid %1 '%2'").arg(name, s);
        }
        return false;
    }
    if (out) {
        *out = n;
    }
    return true;
}

void parseChromeColor(const QStringView v, PageColor& dest)
{
    const QString s = v.toString().trimmed();
    if (!s.isEmpty()) {
        dest.token = s;
    }
}

void applyChromeAttrs(const QXmlStreamAttributes& a, PageChrome& st)
{
    if (a.hasAttribute(QStringLiteral("background"))) {
        parseChromeColor(a.value(QStringLiteral("background")), st.background);
    }
    if (a.hasAttribute(QStringLiteral("foreground"))) {
        parseChromeColor(a.value(QStringLiteral("foreground")), st.foreground);
    }
    if (a.hasAttribute(QStringLiteral("border"))) {
        parseChromeColor(a.value(QStringLiteral("border")), st.borderColor);
    }
    if (a.hasAttribute(QStringLiteral("thickness"))) {
        const PageBox box =
            PageBox::fromToken(a.value(QStringLiteral("thickness")).toString());
        if (box.isSet()) {
            st.thickness = box;
        }
    }
    if (a.hasAttribute(QStringLiteral("radius"))) {
        const PageBox box = PageBox::fromToken(a.value(QStringLiteral("radius")).toString());
        if (box.isSet()) {
            st.radius = box;
        }
    }
    if (a.hasAttribute(QStringLiteral("blur"))) {
        const QString s = a.value(QStringLiteral("blur")).toString().trimmed();
        if (!s.isEmpty()) {
            bool ok = false;
            const double n = s.toDouble(&ok);
            if (ok) {
                st.blur = n;
            }
        }
    }
    if (a.hasAttribute(QStringLiteral("progressStyle"))) {
        const QString s = a.value(QStringLiteral("progressStyle")).toString().trimmed();
        if (!s.isEmpty()) {
            st.progressStyle = ProgressStyle::fromCsv(s);
        }
    }
    if (a.hasAttribute(QStringLiteral("progressColor"))) {
        parseChromeColor(a.value(QStringLiteral("progressColor")), st.progressColor);
    }
}

void applyDwellAttrs(const QXmlStreamAttributes& a, PageDwell& d, QString* error)
{
    if (a.hasAttribute(QStringLiteral("scanGrace"))) {
        const QStringView v = a.value(QStringLiteral("scanGrace"));
        if (!v.trimmed().isEmpty()) {
            int n = 0;
            if (!parseIntAttr(v, 0, &n, error, QStringLiteral("scanGrace"))) {
                return;
            }
            d.scanGrace = n;
        }
    }
    if (a.hasAttribute(QStringLiteral("dwellGrace"))) {
        const QStringView v = a.value(QStringLiteral("dwellGrace"));
        if (!v.trimmed().isEmpty()) {
            int n = 0;
            if (!parseIntAttr(v, 0, &n, error, QStringLiteral("dwellGrace"))) {
                return;
            }
            d.dwellGrace = n;
        }
    }
    if (a.hasAttribute(QStringLiteral("activation"))) {
        const QString s = a.value(QStringLiteral("activation")).toString().trimmed();
        if (!s.isEmpty()) {
            QString err;
            const QVector<int> seq = PageDimParse::parseIntList(s, &err);
            if (!err.isEmpty()) {
                if (error && error->isEmpty()) {
                    *error = err;
                }
                return;
            }
            d.activation = seq;
        }
    }
}

bool skipUnknownOrFail(QXmlStreamReader& xml, const QString& parent, QString* error)
{
    const QString name = xml.name().toString();
    if (error) {
        *error = xmlError(xml, QStringLiteral("Unexpected <%1> in <%2>").arg(name, parent));
    }
    return false;
}

bool applyCommonContent(const QXmlStreamAttributes& a, QString& label, QString& icon,
                        QString& caption, QString& settingKey, QString& activeState,
                        QString& visibleWhen, bool& suspendExempt, bool& actionLoop,
                        QString* error)
{
    if (a.hasAttribute(QStringLiteral("label"))) {
        label = a.value(QStringLiteral("label")).toString();
    }
    if (a.hasAttribute(QStringLiteral("icon"))) {
        icon = a.value(QStringLiteral("icon")).toString();
    }
    if (a.hasAttribute(QStringLiteral("caption"))) {
        caption = a.value(QStringLiteral("caption")).toString();
    }
    if (a.hasAttribute(QStringLiteral("settingKey"))) {
        settingKey = a.value(QStringLiteral("settingKey")).toString();
    }
    if (a.hasAttribute(QStringLiteral("bind"))) {
        activeState = a.value(QStringLiteral("bind")).toString();
    } else if (a.hasAttribute(QStringLiteral("activeState"))) {
        activeState = a.value(QStringLiteral("activeState")).toString();
    }
    if (a.hasAttribute(QStringLiteral("visibleWhen"))) {
        visibleWhen = a.value(QStringLiteral("visibleWhen")).toString();
    }
    if (a.hasAttribute(QStringLiteral("suspendExempt"))) {
        if (!parseBoolAttr(a.value(QStringLiteral("suspendExempt")), false, &suspendExempt, error,
                           QStringLiteral("suspendExempt"))) {
            return false;
        }
    } else if (a.hasAttribute(QStringLiteral("dwellExempt"))) {
        if (!parseBoolAttr(a.value(QStringLiteral("dwellExempt")), false, &suspendExempt, error,
                           QStringLiteral("dwellExempt"))) {
            return false;
        }
    }
    if (a.hasAttribute(QStringLiteral("actionLoop"))) {
        if (!parseBoolAttr(a.value(QStringLiteral("actionLoop")), false, &actionLoop, error,
                           QStringLiteral("actionLoop"))) {
            return false;
        }
    }
    return true;
}

bool parseLayersAttr(const QXmlStreamAttributes& a, QVector<int>& dest, QString* error,
                     const QString& attrName = QStringLiteral("layers"))
{
    if (!a.hasAttribute(attrName)) {
        dest = defaultLayers();
        return true;
    }
    const QString raw = a.value(attrName).toString().trimmed();
    if (raw.isEmpty()) {
        dest = defaultLayers();
        return true;
    }
    if (!parseLayerListStrict(raw, dest) || dest.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Invalid %1 '%2'").arg(attrName, raw);
        }
        return false;
    }
    return true;
}

bool readActionElement(QXmlStreamReader& xml, QVector<PageAction>& actions, QString* error)
{
    const QString name = xml.name().toString();
    const QXmlStreamAttributes a = xml.attributes();
    const QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
    PageAction act;
    QString err;
    if (!parsePageActionElement(name, a, text, act, &err)) {
        if (error) {
            *error = xmlError(xml, err);
        }
        return false;
    }
    actions.push_back(act);
    return true;
}

bool readPhase(QXmlStreamReader& xml, PagePhase& phase, QString* error)
{
    const QXmlStreamAttributes a = xml.attributes();
    QString attrErr;
    if (!takePageActionAttributes(a, phase.actions, &attrErr)) {
        if (error) {
            *error = xmlError(xml, attrErr);
        }
        return false;
    }
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::EndElement && xml.name() == QLatin1String("Phase")) {
            return true;
        }
        if (tok != QXmlStreamReader::StartElement) {
            continue;
        }
        const QString name = xml.name().toString();
        if (isPageActionElementName(name)) {
            if (!readActionElement(xml, phase.actions, error)) {
                return false;
            }
        } else {
            return skipUnknownOrFail(xml, QStringLiteral("Phase"), error);
        }
    }
    if (error) {
        *error = xmlError(xml, QStringLiteral("Unclosed <Phase>"));
    }
    return false;
}

bool readLeafBody(QXmlStreamReader& xml, const QString& parent, QVector<PageAction>& actions,
                  QVector<PagePhase>& phases, QString* error)
{
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::EndElement && xml.name() == parent) {
            return true;
        }
        if (tok != QXmlStreamReader::StartElement) {
            continue;
        }
        const QString name = xml.name().toString();
        if (name == QLatin1String("Phase")) {
            PagePhase phase;
            if (!readPhase(xml, phase, error)) {
                return false;
            }
            phases.push_back(std::move(phase));
        } else if (isPageActionElementName(name)) {
            if (!readActionElement(xml, actions, error)) {
                return false;
            }
        } else {
            return skipUnknownOrFail(xml, parent, error);
        }
    }
    if (error) {
        *error = xmlError(xml, QStringLiteral("Unclosed <%1>").arg(parent));
    }
    return false;
}

bool finishLeafPhases(QXmlStreamReader& xml, const QVector<PageAction>& actions,
                      const QVector<PagePhase>& phases, QString* error)
{
    if (phases.isEmpty()) {
        return true;
    }
    if (!actions.isEmpty()) {
        if (error) {
            *error = xmlError(xml, QStringLiteral("<Phase> children replace cell/zone actions"));
        }
        return false;
    }
    for (const PagePhase& p : phases) {
        if (p.actions.isEmpty()) {
            if (error) {
                *error = xmlError(xml, QStringLiteral("Empty <Phase>"));
            }
            return false;
        }
    }
    return true;
}

bool isDwellSuspendCommand(const QString& name)
{
    return name == QLatin1String("toggleDwellSuspend") || name == QLatin1String("suspendDwell")
           || name == QLatin1String("resumeDwell");
}

void applySuspendExemptFromActions(bool& suspendExempt, const QVector<PageAction>& actions)
{
    if (suspendExempt) {
        return;
    }
    for (const PageAction& a : actions) {
        if (a.type == PageActionType::Command && isDwellSuspendCommand(a.command)) {
            suspendExempt = true;
            return;
        }
    }
}

bool parseOneTrack(const QString& raw, PageTrackSize& out, QString* error)
{
    QString err;
    const QVector<PageTrackSize> list = PageDimParse::parseTrackList(raw, &err);
    if (!err.isEmpty() || list.size() != 1) {
        if (error) {
            *error = err.isEmpty() ? QStringLiteral("Invalid track '%1'").arg(raw) : err;
        }
        return false;
    }
    out = list.at(0);
    return true;
}

void putTrack(QVector<PageTrackSize>& tracks, int index, const PageTrackSize& size)
{
    if (index < 0) {
        return;
    }
    if (tracks.size() <= index) {
        tracks.resize(index + 1);
    }
    tracks[index] = size;
}

struct FlowHint {
    bool rowGiven = false;
    bool colGiven = false;
    bool hasWidth = false;
    bool hasHeight = false;
    PageTrackSize width;
    PageTrackSize height;
};

bool readCell(QXmlStreamReader& xml, PageCell& cell, FlowHint& hint, QString* error)
{
    const QXmlStreamAttributes a = xml.attributes();
    const QString element = xml.name().toString();
    cell.id = a.value(QStringLiteral("id")).toString().trimmed();
    cell.authoredId = !cell.id.isEmpty();
    hint.rowGiven = a.hasAttribute(QStringLiteral("row"));
    hint.colGiven = a.hasAttribute(QStringLiteral("col"));
    const bool colSpanGiven = a.hasAttribute(QStringLiteral("colSpan"));
    if (!parseIntAttr(a.value(QStringLiteral("row")), 0, &cell.row, error, QStringLiteral("row"))
        || !parseIntAttr(a.value(QStringLiteral("col")), 0, &cell.col, error, QStringLiteral("col"))
        || !parseIntAttr(a.value(QStringLiteral("rowSpan")), 1, &cell.rowSpan, error,
                         QStringLiteral("rowSpan"))
        || !parseIntAttr(a.value(QStringLiteral("colSpan")), 1, &cell.colSpan, error,
                         QStringLiteral("colSpan"))) {
        if (error && !error->isEmpty()) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    if (!colSpanGiven && a.hasAttribute(QStringLiteral("span"))) {
        if (!parseIntAttr(a.value(QStringLiteral("span")), 1, &cell.colSpan, error,
                          QStringLiteral("span"))) {
            if (error && !error->isEmpty()) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
    }
    if (a.hasAttribute(QStringLiteral("width"))
        && !parseOneTrack(a.value(QStringLiteral("width")).toString(), hint.width, error)) {
        if (error) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    hint.hasWidth = a.hasAttribute(QStringLiteral("width"));
    if (a.hasAttribute(QStringLiteral("height"))
        && !parseOneTrack(a.value(QStringLiteral("height")).toString(), hint.height, error)) {
        if (error) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    hint.hasHeight = a.hasAttribute(QStringLiteral("height"));
    cell.styleId = a.value(QStringLiteral("style")).toString();
    cell.dwellId = a.value(QStringLiteral("dwell")).toString();
    cell.role = a.value(QStringLiteral("role")).toString();
    if (cell.role.isEmpty()) {
        cell.role = roleForControlElement(element);
    }
    cell.textStyle = a.value(QStringLiteral("textStyle")).toString();
    applyChromeAttrs(a, cell.style);
    applyDwellAttrs(a, cell.dwell, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    if (!applyCommonContent(a, cell.label, cell.icon, cell.caption, cell.settingKey,
                            cell.activeState, cell.visibleWhen, cell.suspendExempt, cell.actionLoop,
                            error)) {
        if (error && !error->isEmpty()) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    QString attrErr;
    if (!takePageActionAttributes(a, cell.actions, &attrErr)) {
        if (error) {
            *error = xmlError(xml, attrErr);
        }
        return false;
    }
    if (!readLeafBody(xml, element, cell.actions, cell.phases, error)) {
        return false;
    }
    if (!finishLeafPhases(xml, cell.actions, cell.phases, error)) {
        return false;
    }
    applySuspendExemptFromActions(cell.suspendExempt, cell.actions);
    for (const PagePhase& p : cell.phases) {
        applySuspendExemptFromActions(cell.suspendExempt, p.actions);
    }
    return true;
}

bool readGrid(QXmlStreamReader& xml, PageGrid& grid, bool nested, FlowHint* placed, QString* error);

void applyPlacement(const QXmlStreamAttributes& a, bool& desktopMode, PageAnchor& anchor,
                    PageDimPair& offset, PageDimPair& size, QString* error)
{
    if (!parseBoolAttr(a.value(QStringLiteral("desktopMode")), false, &desktopMode, error,
                       QStringLiteral("desktopMode"))) {
        return;
    }
    if (a.hasAttribute(QStringLiteral("anchor"))) {
        bool ok = true;
        anchor = PageDimParse::parseAnchor(a.value(QStringLiteral("anchor")).toString(), &ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Unknown anchor '%1'")
                             .arg(a.value(QStringLiteral("anchor")).toString());
            }
            return;
        }
    }
    if (a.hasAttribute(QStringLiteral("offset"))) {
        QString err;
        offset = PageDimParse::parsePair(a.value(QStringLiteral("offset")).toString(), &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = err;
            }
            return;
        }
    }
    if (a.hasAttribute(QStringLiteral("size"))) {
        QString err;
        size = PageDimParse::parsePair(a.value(QStringLiteral("size")).toString(), &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = err;
            }
        }
    }
}

bool readGrid(QXmlStreamReader& xml, PageGrid& grid, bool nested, FlowHint* placed, QString* error)
{
    const QString endName = xml.name().toString();
    grid.flow = pageFlowFromElement(endName);
    grid.nested = nested || endName == QLatin1String("SubGrid");
    const QXmlStreamAttributes a = xml.attributes();
    grid.id = a.value(QStringLiteral("id")).toString().trimmed();
    grid.authoredId = !grid.id.isEmpty();
    const bool rowsGiven = a.hasAttribute(QStringLiteral("rows"));
    const bool columnsGiven = a.hasAttribute(QStringLiteral("columns"));
    auto takeInt = [&](const QString& name, int fallback, int* dest) {
        if (!parseIntAttr(a.value(name), fallback, dest, error, name)) {
            if (error && !error->isEmpty()) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
        return true;
    };
    if (!takeInt(QStringLiteral("rows"), 1, &grid.rows)
        || !takeInt(QStringLiteral("columns"), 1, &grid.columns)
        || !takeInt(QStringLiteral("row"), 0, &grid.row)
        || !takeInt(QStringLiteral("col"), 0, &grid.col)
        || !takeInt(QStringLiteral("rowSpan"), 1, &grid.rowSpan)
        || !takeInt(QStringLiteral("colSpan"), 1, &grid.colSpan)
        || !takeInt(QStringLiteral("gap"), 0, &grid.gapPx)
        || !takeInt(QStringLiteral("margin"), 0, &grid.marginPx)) {
        return false;
    }
    if (!a.hasAttribute(QStringLiteral("colSpan")) && a.hasAttribute(QStringLiteral("span"))) {
        if (!parseIntAttr(a.value(QStringLiteral("span")), 1, &grid.colSpan, error,
                          QStringLiteral("span"))) {
            if (error && !error->isEmpty()) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
    }
    if (a.hasAttribute(QStringLiteral("padding"))) {
        if (!parseIntAttr(a.value(QStringLiteral("padding")), 0, &grid.marginPx, error,
                          QStringLiteral("padding"))) {
            if (error && !error->isEmpty()) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
    }
    if (grid.rows < 1) {
        grid.rows = 1;
    }
    if (grid.columns < 1) {
        grid.columns = 1;
    }
    auto loadTracks = [&](const QString& name, QVector<PageTrackSize>& dest) -> bool {
        if (!a.hasAttribute(name)) {
            return true;
        }
        QString err;
        dest = PageDimParse::parseTrackList(a.value(name).toString(), &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = xmlError(xml, err);
            }
            return false;
        }
        return true;
    };
    if (!loadTracks(QStringLiteral("rowHeights"), grid.rowTracks)
        || !loadTracks(QStringLiteral("columnWidths"), grid.columnTracks)) {
        return false;
    }
    auto takeBool = [&](const QString& name, bool* dest) {
        if (!parseBoolAttr(a.value(name), false, dest, error, name)) {
            if (error && !error->isEmpty()) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
        return true;
    };
    if (!takeBool(QStringLiteral("drawerMotion"), &grid.drawerMotion)
        || !takeBool(QStringLiteral("autoClose"), &grid.autoClose)
        || !takeBool(QStringLiteral("scroll"), &grid.scroll)) {
        return false;
    }
    if (!parseLayersAttr(a, grid.layers, error)) {
        return false;
    }
    if (!takeBool(QStringLiteral("shell"), &grid.shell)) {
        return false;
    }
    FlowHint place;
    place.rowGiven = a.hasAttribute(QStringLiteral("row"));
    place.colGiven = a.hasAttribute(QStringLiteral("col"));
    if (a.hasAttribute(QStringLiteral("width"))) {
        if (!parseOneTrack(a.value(QStringLiteral("width")).toString(), place.width, error)) {
            if (error) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
        place.hasWidth = true;
    }
    if (a.hasAttribute(QStringLiteral("height"))) {
        if (!parseOneTrack(a.value(QStringLiteral("height")).toString(), place.height, error)) {
            if (error) {
                *error = xmlError(xml, *error);
            }
            return false;
        }
        place.hasHeight = true;
    }
    if (placed) {
        *placed = place;
    }
    grid.styleId = a.value(QStringLiteral("style")).toString();
    grid.dwellId = a.value(QStringLiteral("dwell")).toString();
    grid.src = a.value(QStringLiteral("src")).toString().trimmed();
    grid.srcGrid = a.value(QStringLiteral("grid")).toString().trimmed();
    applyChromeAttrs(a, grid.style);
    applyDwellAttrs(a, grid.dwell, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    applyPlacement(a, grid.desktopMode, grid.anchor, grid.offset, grid.size, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    if (!nested && !grid.size.isSet()) {
        grid.size.x = PageDim::pixels(600);
        grid.size.y = PageDim::pixels(400);
    }
    if (!nested && !grid.offset.isSet()) {
        grid.offset.x = PageDim::pixels(0);
        grid.offset.y = PageDim::pixels(0);
    }

    const bool flowPlace = grid.flow == PageFlow::Row || grid.flow == PageFlow::Column
                           || grid.flow == PageFlow::Card;
    int nextCol = 0;
    int nextRow = 0;
    auto placeChild = [&](int& row, int& col, int rowSpan, int colSpan, const FlowHint& hint) {
        if (flowPlace && !hint.rowGiven && !hint.colGiven) {
            if (grid.flow == PageFlow::Row) {
                row = 0;
                col = nextCol;
                nextCol += qMax(1, colSpan);
            } else {
                col = 0;
                row = nextRow;
                nextRow += qMax(1, rowSpan);
            }
        }
        if (hint.hasWidth) {
            putTrack(grid.columnTracks, col, hint.width);
        }
        if (hint.hasHeight) {
            putTrack(grid.rowTracks, row, hint.height);
        }
    };
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::EndElement && xml.name() == endName) {
            if (grid.flow != PageFlow::Mesh && (!rowsGiven || !columnsGiven)) {
                int rows = grid.rowTracks.size();
                int columns = grid.columnTracks.size();
                for (const PageCell& cell : grid.cells) {
                    rows = qMax(rows, cell.row + qMax(1, cell.rowSpan));
                    columns = qMax(columns, cell.col + qMax(1, cell.colSpan));
                }
                for (const PageGrid& sub : grid.subGrids) {
                    rows = qMax(rows, sub.row + qMax(1, sub.rowSpan));
                    columns = qMax(columns, sub.col + qMax(1, sub.colSpan));
                }
                if (!rowsGiven && rows > 0) {
                    grid.rows = rows;
                }
                if (!columnsGiven && columns > 0) {
                    grid.columns = columns;
                }
            }
            return true;
        }
        if (tok != QXmlStreamReader::StartElement) {
            continue;
        }
        const QString name = xml.name().toString();
        if (isControlElement(name) || isFlowElement(name)) {
            if (!grid.src.isEmpty()) {
                if (error) {
                    *error = xmlError(xml, QStringLiteral("src grid cannot have children"));
                }
                return false;
            }
        }
        if (isControlElement(name)) {
            PageCell cell;
            FlowHint hint;
            if (!readCell(xml, cell, hint, error)) {
                return false;
            }
            placeChild(cell.row, cell.col, cell.rowSpan, cell.colSpan, hint);
            grid.cells.push_back(cell);
        } else if (isFlowElement(name)) {
            PageGrid sub;
            FlowHint hint;
            if (!readGrid(xml, sub, true, &hint, error)) {
                return false;
            }
            placeChild(sub.row, sub.col, sub.rowSpan, sub.colSpan, hint);
            grid.subGrids.push_back(std::move(sub));
        } else {
            return skipUnknownOrFail(xml, endName, error);
        }
    }
    if (error) {
        *error = xmlError(xml, QStringLiteral("Unclosed <%1>").arg(endName));
    }
    return false;
}

bool readZone(QXmlStreamReader& xml, PageZone& zone, QString* error)
{
    const QXmlStreamAttributes a = xml.attributes();
    zone.id = a.value(QStringLiteral("id")).toString().trimmed();
    zone.authoredId = !zone.id.isEmpty();
    zone.styleId = a.value(QStringLiteral("style")).toString();
    zone.dwellId = a.value(QStringLiteral("dwell")).toString();
    applyChromeAttrs(a, zone.style);
    applyDwellAttrs(a, zone.dwell, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    applyPlacement(a, zone.desktopMode, zone.anchor, zone.offset, zone.size, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    if (a.hasAttribute(QStringLiteral("dwellOffset"))) {
        QString err;
        zone.dwellOffset =
            PageDimParse::parsePair(a.value(QStringLiteral("dwellOffset")).toString(), &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = err;
            }
            return false;
        }
    }
    if (a.hasAttribute(QStringLiteral("dwellSize"))) {
        QString err;
        zone.dwellSize =
            PageDimParse::parsePair(a.value(QStringLiteral("dwellSize")).toString(), &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = err;
            }
            return false;
        }
    }
    if (!applyCommonContent(a, zone.label, zone.icon, zone.caption, zone.settingKey,
                            zone.activeState, zone.visibleWhen, zone.suspendExempt, zone.actionLoop,
                            error)) {
        if (error && !error->isEmpty()) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    if (!parseLayersAttr(a, zone.layers, error)) {
        return false;
    }
    if (!parseBoolAttr(a.value(QStringLiteral("shell")), false, &zone.shell, error,
                       QStringLiteral("shell"))) {
        if (error && !error->isEmpty()) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    if (!zone.size.isSet()) {
        zone.size.x = PageDim::pixels(200);
        zone.size.y = PageDim::pixels(200);
    }
    if (!zone.offset.isSet()) {
        zone.offset.x = PageDim::pixels(0);
        zone.offset.y = PageDim::pixels(0);
    }
    if (!zone.dwellOffset.isSet()) {
        zone.dwellOffset.x = PageDim::pixels(0);
        zone.dwellOffset.y = PageDim::pixels(0);
    }
    if (!zone.dwellSize.isSet()) {
        zone.dwellSize = zone.size;
    }
    QString attrErr;
    if (!takePageActionAttributes(a, zone.actions, &attrErr)) {
        if (error) {
            *error = xmlError(xml, attrErr);
        }
        return false;
    }
    if (!readLeafBody(xml, QStringLiteral("Zone"), zone.actions, zone.phases, error)) {
        return false;
    }
    if (!finishLeafPhases(xml, zone.actions, zone.phases, error)) {
        return false;
    }
    applySuspendExemptFromActions(zone.suspendExempt, zone.actions);
    for (const PagePhase& p : zone.phases) {
        applySuspendExemptFromActions(zone.suspendExempt, p.actions);
    }
    return true;
}

bool readPage(QXmlStreamReader& xml, PageDocument& out, QString* error)
{
    const QXmlStreamAttributes a = xml.attributes();
    out.id = a.value(QStringLiteral("id")).toString().trimmed();
    out.name = a.value(QStringLiteral("name")).toString();
    if (out.name.isEmpty()) {
        out.name = out.id;
    }
    if (!parseBoolAttr(a.value(QStringLiteral("master")), false, &out.master, error,
                       QStringLiteral("master"))
        || !parseBoolAttr(a.value(QStringLiteral("autoClose")), false, &out.autoClose, error,
                          QStringLiteral("autoClose"))) {
        if (error && !error->isEmpty()) {
            *error = xmlError(xml, *error);
        }
        return false;
    }
    if (a.hasAttribute(QStringLiteral("showLayers"))) {
        if (!parseLayersAttr(a, out.showLayers, error, QStringLiteral("showLayers"))) {
            return false;
        }
    }
    applyChromeAttrs(a, out.style);
    applyDwellAttrs(a, out.dwell, error);
    if (error && !error->isEmpty()) {
        return false;
    }
    if (out.id.isEmpty()) {
        if (error) {
            *error = xmlError(xml, QStringLiteral("Page requires id"));
        }
        return false;
    }

    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::EndElement && xml.name() == QLatin1String("Page")) {
            return true;
        }
        if (tok != QXmlStreamReader::StartElement) {
            continue;
        }
        const QString name = xml.name().toString();
        if (name == QLatin1String("Style")) {
            const QXmlStreamAttributes sa = xml.attributes();
            PageChrome st;
            applyChromeAttrs(sa, st);
            const QString sid = sa.value(QStringLiteral("id")).toString().trimmed();
            if (sid.isEmpty()) {
                out.style = out.style.withOverrides(st);
            } else {
                out.styles.insert(sid, st);
            }
            xml.skipCurrentElement();
        } else if (name == QLatin1String("Dwell")) {
            const QXmlStreamAttributes da = xml.attributes();
            PageDwell d;
            applyDwellAttrs(da, d, error);
            if (error && !error->isEmpty()) {
                return false;
            }
            const QString did = da.value(QStringLiteral("id")).toString().trimmed();
            if (did.isEmpty()) {
                out.dwell = out.dwell.withOverrides(d);
            } else {
                out.dwells.insert(did, d);
            }
            xml.skipCurrentElement();
        } else if (isFlowElement(name)) {
            PageGrid g;
            if (!readGrid(xml, g, false, nullptr, error)) {
                return false;
            }
            out.grids.push_back(std::move(g));
        } else if (name == QLatin1String("Zone")) {
            PageZone z;
            if (!readZone(xml, z, error)) {
                return false;
            }
            out.zones.push_back(z);
        } else {
            return skipUnknownOrFail(xml, QStringLiteral("Page"), error);
        }
    }
    if (error) {
        *error = xmlError(xml, QStringLiteral("Unclosed <Page>"));
    }
    return false;
}

} // namespace

bool PageLoader::loadFromXml(const QByteArray& xmlBytes, PageDocument& out, QString* error)
{
    out = PageDocument{};
    QXmlStreamReader xml(xmlBytes);
    while (!xml.atEnd()) {
        const auto tok = xml.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            if (xml.name() != QLatin1String("Page")) {
                if (error) {
                    *error = xmlError(xml, QStringLiteral("Root element must be <Page>"));
                }
                return false;
            }
            if (!readPage(xml, out, error)) {
                return false;
            }
            PageEdit::assignMissingIds(out);
            return true;
        }
    }
    if (xml.hasError()) {
        if (error) {
            *error = xml.errorString();
        }
        return false;
    }
    if (error) {
        *error = QStringLiteral("No <Page> root");
    }
    return false;
}

bool PageLoader::loadFromFile(const QString& path, PageDocument& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open %1").arg(path);
        }
        return false;
    }
    return loadFromXml(f.readAll(), out, error);
}

} // namespace gazer
