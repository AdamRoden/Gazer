#include "layout/LayoutLoader.h"

#include "utils/Log.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gazer {

namespace {

void parseDwellObject(const QJsonObject& dwell, LayoutDwellConfig& out)
{
    out.sectionPresent = true;
    out.enabled = dwell.value(QStringLiteral("enabled")).toBool(out.enabled);
    out.repeatMs = dwell.value(QStringLiteral("repeatMs")).toInt(out.repeatMs);

    if (dwell.contains(QStringLiteral("progressStyle"))) {
        out.hasProgressStyle = true;
        out.progressStyle =
            dwell.value(QStringLiteral("progressStyle")).toString(QStringLiteral("radial"));
    }
    if (dwell.contains(QStringLiteral("graceMs"))) {
        out.hasGrace = true;
        out.graceMs = dwell.value(QStringLiteral("graceMs")).toInt(-1);
    }
    if (dwell.contains(QStringLiteral("scanGraceMs"))) {
        out.hasScanGrace = true;
        out.scanGraceMs = dwell.value(QStringLiteral("scanGraceMs")).toInt(-1);
    }
    if (dwell.contains(QStringLiteral("progressColor"))) {
        out.hasProgressColor = true;
        out.progressColor = dwell.value(QStringLiteral("progressColor")).toString();
    }
    if (dwell.contains(QStringLiteral("fillColor"))) {
        out.hasFillColor = true;
        out.fillColor = dwell.value(QStringLiteral("fillColor")).toString();
    }
    if (dwell.contains(QStringLiteral("borderColor"))) {
        out.hasBorderColor = true;
        out.borderColor = dwell.value(QStringLiteral("borderColor")).toString();
    }
    if (dwell.contains(QStringLiteral("flashBorderColor"))) {
        out.hasFlashBorderColor = true;
        out.flashBorderColor = dwell.value(QStringLiteral("flashBorderColor")).toString();
    }
    if (dwell.contains(QStringLiteral("flashFillColor"))) {
        out.hasFlashFillColor = true;
        out.flashFillColor = dwell.value(QStringLiteral("flashFillColor")).toString();
    }
    if (dwell.contains(QStringLiteral("flashMs"))) {
        out.hasFlashMs = true;
        out.flashMs = dwell.value(QStringLiteral("flashMs")).toInt(-1);
    }

    // ms may be a single int or an array of progressive dwell steps.
    if (dwell.contains(QStringLiteral("ms"))) {
        out.hasTiming = true;
        const QJsonValue msVal = dwell.value(QStringLiteral("ms"));
        out.msSequence.clear();
        if (msVal.isArray()) {
            for (const QJsonValue& v : msVal.toArray()) {
                const int n = v.toInt(0);
                if (n > 0) {
                    out.msSequence.push_back(n);
                }
            }
            if (!out.msSequence.isEmpty()) {
                out.ms = out.msSequence.first();
            } else {
                out.ms = 800;
                out.msSequence = {800};
            }
        } else {
            out.ms = msVal.toInt(800);
            out.msSequence = {out.ms};
        }
    }
}

LayoutAction::Type parseActionType(const QString& s)
{
    if (s == QLatin1String("speak")) {
        return LayoutAction::Type::Speak;
    }
    if (s == QLatin1String("typeText")) {
        return LayoutAction::Type::TypeText;
    }
    if (s == QLatin1String("loadLayout")) {
        return LayoutAction::Type::LoadLayout;
    }
    if (s == QLatin1String("openLayout")) {
        return LayoutAction::Type::OpenLayout;
    }
    if (s == QLatin1String("closeLayout")) {
        return LayoutAction::Type::CloseLayout;
    }
    if (s == QLatin1String("command")) {
        return LayoutAction::Type::Command;
    }
    if (s == QLatin1String("script")) {
        return LayoutAction::Type::Script;
    }
    return LayoutAction::Type::Unknown;
}

std::optional<QColor> parseColor(const QJsonValue& v)
{
    if (!v.isString()) {
        return std::nullopt;
    }
    const QColor c(v.toString());
    if (!c.isValid()) {
        return std::nullopt;
    }
    return c;
}

BoundsMode parseBoundsMode(const QString& s, BoundsMode fallback = BoundsMode::Desktop)
{
    const QString m = s.trimmed().toLower();
    if (m == QLatin1String("screen") || m == QLatin1String("full")
        || m == QLatin1String("geometry")) {
        return BoundsMode::Screen;
    }
    if (m == QLatin1String("desktop") || m == QLatin1String("available")
        || m == QLatin1String("workarea") || m == QLatin1String("work")) {
        return BoundsMode::Desktop;
    }
    return fallback;
}

void parseChromeStyle(const QJsonObject& st, LayoutChromeStyle& out)
{
    if (st.contains(QStringLiteral("background"))) {
        out.background = parseColor(st.value(QStringLiteral("background")));
    }
    if (st.contains(QStringLiteral("foreground"))) {
        out.foreground = parseColor(st.value(QStringLiteral("foreground")));
    }
    if (st.contains(QStringLiteral("borderColor")) || st.contains(QStringLiteral("border"))) {
        const QJsonValue bc = st.contains(QStringLiteral("borderColor"))
                                  ? st.value(QStringLiteral("borderColor"))
                                  : st.value(QStringLiteral("border"));
        out.borderColor = parseColor(bc);
    }
    if (st.contains(QStringLiteral("borderWidth")) || st.contains(QStringLiteral("borderThickness"))
        || st.contains(QStringLiteral("thickness"))) {
        const QJsonValue v = st.contains(QStringLiteral("borderWidth"))
                                 ? st.value(QStringLiteral("borderWidth"))
                                 : (st.contains(QStringLiteral("borderThickness"))
                                        ? st.value(QStringLiteral("borderThickness"))
                                        : st.value(QStringLiteral("thickness")));
        out.borderWidth = v.toDouble(1.5);
    }
    if (st.contains(QStringLiteral("radius")) || st.contains(QStringLiteral("borderRadius"))) {
        const QJsonValue v = st.contains(QStringLiteral("radius"))
                                 ? st.value(QStringLiteral("radius"))
                                 : st.value(QStringLiteral("borderRadius"));
        out.radius = v.toDouble(12.0);
    }
}

bool parseAction(const QJsonObject& obj, LayoutAction& out, QString* error)
{
    const QString typeStr = obj.value(QStringLiteral("type")).toString();
    out.type = parseActionType(typeStr);
    if (out.type == LayoutAction::Type::Unknown) {
        if (error) {
            *error = QStringLiteral("Unknown action type: %1").arg(typeStr);
        }
        return false;
    }
    out.text = obj.value(QStringLiteral("text")).toString();
    out.layoutId = obj.value(QStringLiteral("layoutId")).toString();
    out.name = obj.value(QStringLiteral("name")).toString();
    out.source = obj.value(QStringLiteral("source")).toString();
    out.delayMs = obj.value(QStringLiteral("delayMs")).toInt(0);

    switch (out.type) {
    case LayoutAction::Type::Speak:
        if (out.text.isEmpty()) {
            if (error) {
                *error = QStringLiteral("speak action requires text");
            }
            return false;
        }
        break;
    case LayoutAction::Type::TypeText:
        // empty text allowed (no-op) for reserved slots
        break;
    case LayoutAction::Type::LoadLayout:
    case LayoutAction::Type::OpenLayout:
        if (out.layoutId.isEmpty()) {
            if (error) {
                *error = QStringLiteral("%1 action requires layoutId")
                             .arg(typeStr);
            }
            return false;
        }
        break;
    case LayoutAction::Type::CloseLayout:
        break;
    case LayoutAction::Type::Command:
        if (out.name.isEmpty()) {
            if (error) {
                *error = QStringLiteral("command action requires name");
            }
            return false;
        }
        break;
    case LayoutAction::Type::Script:
        // source may be empty in stubs
        break;
    case LayoutAction::Type::Unknown:
        break;
    }
    return true;
}

bool parseActionList(const QJsonValue& v, QVector<LayoutAction>& out, QString* error)
{
    if (!v.isArray()) {
        if (error) {
            *error = QStringLiteral("Expected action array");
        }
        return false;
    }
    for (const QJsonValue& av : v.toArray()) {
        if (!av.isObject()) {
            continue;
        }
        LayoutAction a;
        if (!parseAction(av.toObject(), a, error)) {
            return false;
        }
        out.push_back(std::move(a));
    }
    return true;
}

/// Parse dim: prefer *Px key; else bare key as percent (number or "N%"); optional default.
void parseDim(const QJsonObject& obj, const QString& baseKey, DimSpec& out)
{
    const QString pxKey = baseKey + QStringLiteral("Px");
    if (obj.contains(pxKey)) {
        out.unit = DimSpec::Unit::Pixels;
        out.value = obj.value(pxKey).toDouble(0);
        return;
    }
    if (!obj.contains(baseKey)) {
        return;
    }
    const QJsonValue v = obj.value(baseKey);
    if (v.isString()) {
        QString s = v.toString().trimmed();
        if (s.endsWith(QLatin1Char('%'))) {
            s.chop(1);
            out.unit = DimSpec::Unit::Percent;
            out.value = s.toDouble();
            return;
        }
        // bare numeric string → percent by default
        bool ok = false;
        const double n = s.toDouble(&ok);
        if (ok) {
            out.unit = DimSpec::Unit::Percent;
            out.value = n;
        }
        return;
    }
    if (v.isDouble() || v.isBool()) {
        out.unit = DimSpec::Unit::Percent;
        out.value = v.toDouble(0);
    }
}

LayoutDwellRegion::ScreenAnchor parseScreenAnchor(const QString& anchor)
{
    const QString a = anchor.toLower();
    using SA = LayoutDwellRegion::ScreenAnchor;
    if (a == QLatin1String("top")) {
        return SA::Top;
    }
    if (a == QLatin1String("bottom")) {
        return SA::Bottom;
    }
    if (a == QLatin1String("left")) {
        return SA::Left;
    }
    if (a == QLatin1String("right")) {
        return SA::Right;
    }
    if (a == QLatin1String("topleft")) {
        return SA::TopLeft;
    }
    if (a == QLatin1String("topright")) {
        return SA::TopRight;
    }
    if (a == QLatin1String("bottomleft")) {
        return SA::BottomLeft;
    }
    if (a == QLatin1String("bottomright")) {
        return SA::BottomRight;
    }
    if (a == QLatin1String("topcenter")) {
        return SA::TopCenter;
    }
    if (a == QLatin1String("bottomcenter")) {
        return SA::BottomCenter;
    }
    if (a == QLatin1String("leftcenter")) {
        return SA::LeftCenter;
    }
    if (a == QLatin1String("rightcenter")) {
        return SA::RightCenter;
    }
    return SA::None;
}

} // namespace

bool LayoutLoader::loadFromFile(const QString& path, LayoutDocument& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open layout: %1").arg(path);
        }
        return false;
    }
    return loadFromJson(f.readAll(), out, error);
}

bool LayoutLoader::loadFromJson(const QByteArray& json, LayoutDocument& out, QString* error)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON parse error: %1").arg(pe.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    LayoutDocument layout;
    layout.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    layout.id = root.value(QStringLiteral("id")).toString();
    layout.name = root.value(QStringLiteral("name")).toString();
    layout.description = root.value(QStringLiteral("description")).toString();
    {
        const QString style = root.value(QStringLiteral("uiStyle")).toString().toLower();
        if (style == QLatin1String("fluent") || style == QLatin1String("material")) {
            layout.uiStyle = LayoutUiStyle::Fluent;
        } else {
            layout.uiStyle = LayoutUiStyle::Default;
        }
    }

    if (layout.id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Layout missing required field: id");
        }
        return false;
    }

    // Optional session: { "role": "masterShell"|"secondary", "masterGroup", "isHome",
    //                     "collapseLayoutId", "expandLayoutId" }
    if (root.contains(QStringLiteral("session"))) {
        const QJsonObject sess = root.value(QStringLiteral("session")).toObject();
        const QString role = sess.value(QStringLiteral("role")).toString().toLower();
        if (role == QLatin1String("mastershell") || role == QLatin1String("master")) {
            layout.session.role = LayoutRole::MasterShell;
        } else {
            layout.session.role = LayoutRole::Secondary;
        }
        layout.session.masterGroup = sess.value(QStringLiteral("masterGroup")).toString();
        layout.session.isHome = sess.value(QStringLiteral("isHome")).toBool(false);
        layout.session.collapseLayoutId =
            sess.value(QStringLiteral("collapseLayoutId")).toString();
        layout.session.expandLayoutId = sess.value(QStringLiteral("expandLayoutId")).toString();
        layout.session.hideUntilGazeReveal =
            sess.value(QStringLiteral("hideUntilGazeReveal")).toBool(false);
        if (layout.session.isMasterShell() && layout.session.masterGroup.isEmpty()) {
            layout.session.masterGroup = QStringLiteral("default");
        }
    }

    // boundsMode: "desktop" (availableGeometry) or "screen" (full geometry)
    if (root.contains(QStringLiteral("boundsMode")) || root.contains(QStringLiteral("bounds"))) {
        layout.hasBoundsMode = true;
        const QString raw = root.contains(QStringLiteral("boundsMode"))
                                ? root.value(QStringLiteral("boundsMode")).toString()
                                : root.value(QStringLiteral("bounds")).toString();
        layout.boundsMode = parseBoundsMode(raw);
    }

    if (root.contains(QStringLiteral("autoClose"))) {
        layout.autoClose = root.value(QStringLiteral("autoClose")).toBool(true);
    }
    if (root.contains(QStringLiteral("autoCloseIdleMs"))) {
        layout.autoCloseIdleMs = root.value(QStringLiteral("autoCloseIdleMs")).toInt(-1);
    }
    if (root.contains(QStringLiteral("autoCloseFadeMs"))) {
        layout.autoCloseFadeMs = root.value(QStringLiteral("autoCloseFadeMs")).toInt(-1);
    }
    // Master shells stay open by default (home / dock).
    if (layout.session.isMasterShell() && !root.contains(QStringLiteral("autoClose"))) {
        layout.autoClose = false;
    }

    // Grid is optional — default 1×1; expanded later to fit items if omitted/undersized.
    // marginPx defaults to 0 (flush) whether grid is present or omitted.
    if (root.contains(QStringLiteral("grid"))) {
        const QJsonObject grid = root.value(QStringLiteral("grid")).toObject();
        layout.grid.columns = grid.value(QStringLiteral("columns")).toInt(1);
        layout.grid.rows = grid.value(QStringLiteral("rows")).toInt(1);
        layout.grid.gapPx = grid.value(QStringLiteral("gapPx")).toInt(8);
        layout.grid.marginPx = grid.value(QStringLiteral("marginPx")).toInt(0);
        layout.grid.unitRows = grid.value(QStringLiteral("unitRows")).toBool(false);
    } else {
        layout.grid.columns = 1;
        layout.grid.rows = 1;
        layout.grid.gapPx = 8;
        layout.grid.marginPx = 0;
        layout.grid.unitRows = false;
    }
    if (layout.grid.columns < 1) {
        layout.grid.columns = 1;
    }
    if (layout.grid.rows < 1) {
        layout.grid.rows = 1;
    }

    // Optional layout-level dwell override (overrides AppSettings when present).
    if (root.contains(QStringLiteral("dwell"))) {
        parseDwellObject(root.value(QStringLiteral("dwell")).toObject(), layout.dwell);
    }

    // Optional window: anchor, size/pos as % or *Px, marginPx, hidden.
    // Omit "window": show board if any grid items exist; hide pure items-only layouts.
    // "hidden": true → never show board chrome (edge/unbounded-only shells).
    if (root.contains(QStringLiteral("window"))) {
        layout.placement.specified = true;
        const QJsonObject win = root.value(QStringLiteral("window")).toObject();
        layout.placement.hidden = win.value(QStringLiteral("hidden")).toBool(false)
                                  || win.value(QStringLiteral("visible")).toBool(true) == false;
        const QString anchor = win.value(QStringLiteral("anchor")).toString().toLower();
        if (anchor == QLatin1String("topleft")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::TopLeft;
        } else if (anchor == QLatin1String("topcenter")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::TopCenter;
        } else if (anchor == QLatin1String("topright")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::TopRight;
        } else if (anchor == QLatin1String("center")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::Center;
        } else if (anchor == QLatin1String("leftcenter") || anchor == QLatin1String("centerleft")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::LeftCenter;
        } else if (anchor == QLatin1String("rightcenter") || anchor == QLatin1String("centerright")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::RightCenter;
        } else if (anchor == QLatin1String("bottomleft")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::BottomLeft;
        } else if (anchor == QLatin1String("bottomcenter")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::BottomCenter;
        } else if (anchor == QLatin1String("bottomright")) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::BottomRight;
        } else if (anchor == QLatin1String("default") || anchor.isEmpty()) {
            layout.placement.anchor = LayoutWindowPlacement::Anchor::Default;
        } else {
            GAZER_WARN << "Unknown window.anchor" << anchor << "— using default";
        }
        parseDim(win, QStringLiteral("width"), layout.placement.width);
        parseDim(win, QStringLiteral("height"), layout.placement.height);
        parseDim(win, QStringLiteral("x"), layout.placement.x);
        parseDim(win, QStringLiteral("y"), layout.placement.y);
        // Legacy: widthPx/heightPx alone → DimSpec pixels (sole runtime representation).
        if (!layout.placement.width.isSet() && win.contains(QStringLiteral("widthPx"))) {
            layout.placement.width = DimSpec::pixels(win.value(QStringLiteral("widthPx")).toDouble(0));
        }
        if (!layout.placement.height.isSet() && win.contains(QStringLiteral("heightPx"))) {
            layout.placement.height =
                DimSpec::pixels(win.value(QStringLiteral("heightPx")).toDouble(0));
        }
        layout.placement.marginPx = win.value(QStringLiteral("marginPx")).toInt(0);

        if (win.contains(QStringLiteral("boundsMode")) || win.contains(QStringLiteral("bounds"))) {
            layout.placement.hasBoundsMode = true;
            const QString raw = win.contains(QStringLiteral("boundsMode"))
                                    ? win.value(QStringLiteral("boundsMode")).toString()
                                    : win.value(QStringLiteral("bounds")).toString();
            layout.placement.boundsMode =
                parseBoundsMode(raw, layout.effectiveBoundsMode());
        }
        if (win.contains(QStringLiteral("style"))) {
            parseChromeStyle(win.value(QStringLiteral("style")).toObject(), layout.placement.style);
        } else {
            parseChromeStyle(win, layout.placement.style);
        }
    }

    // Root-level style applies to window chrome when window.style omitted.
    if (root.contains(QStringLiteral("style")) && !layout.placement.style.hasAny()) {
        parseChromeStyle(root.value(QStringLiteral("style")).toObject(), layout.placement.style);
    }

    // Lifecycle action arrays
    if (root.contains(QStringLiteral("onOpen"))) {
        if (!parseActionList(root.value(QStringLiteral("onOpen")), layout.onOpen, error)) {
            return false;
        }
    }
    if (root.contains(QStringLiteral("onLoad"))) {
        if (!parseActionList(root.value(QStringLiteral("onLoad")), layout.onLoad, error)) {
            return false;
        }
    }
    if (root.contains(QStringLiteral("onClose"))) {
        if (!parseActionList(root.value(QStringLiteral("onClose")), layout.onClose, error)) {
            return false;
        }
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    layout.items.reserve(items.size());
    int maxRow = 0;
    int maxCol = 0;
    for (const QJsonValue& iv : items) {
        if (!iv.isObject()) {
            continue;
        }
        const QJsonObject io = iv.toObject();
        LayoutItem item;
        item.id = io.value(QStringLiteral("id")).toString();
        item.label = io.value(QStringLiteral("label")).toString();
        item.caption = io.value(QStringLiteral("caption")).toString();
        item.tooltip = io.value(QStringLiteral("tooltip")).toString();
        item.settingKey = io.value(QStringLiteral("settingKey")).toString();
        item.activeState = io.value(QStringLiteral("activeState")).toString();
        item.icon = io.value(QStringLiteral("icon")).toString();
        // role: "label" | interactive: false → static non-activator
        if (io.contains(QStringLiteral("interactive"))) {
            item.interactive = io.value(QStringLiteral("interactive")).toBool(true);
        } else {
            const QString role = io.value(QStringLiteral("role")).toString().toLower();
            item.interactive = !(role == QLatin1String("label") || role == QLatin1String("display")
                                 || role == QLatin1String("value"));
        }
        item.dwellExempt = io.value(QStringLiteral("dwellExempt")).toBool(false)
                           || io.value(QStringLiteral("role")).toString().toLower()
                                  == QLatin1String("dwellExempt");
        item.row = io.value(QStringLiteral("row")).toInt(0);
        item.col = io.value(QStringLiteral("col")).toInt(0);
        item.rowSpan = io.value(QStringLiteral("rowSpan")).toInt(1);
        item.colSpan = io.value(QStringLiteral("colSpan")).toInt(1);
        // Voice OSK relative width (letter unit = 1). Accept "u" or "widthUnits".
        if (io.contains(QStringLiteral("u"))) {
            item.widthUnits = io.value(QStringLiteral("u")).toDouble(0.0);
        } else if (io.contains(QStringLiteral("widthUnits"))) {
            item.widthUnits = io.value(QStringLiteral("widthUnits")).toDouble(0.0);
        }
        if (item.widthUnits > 0.0) {
            layout.grid.unitRows = true;
        }
        if (item.id.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Layout item missing id");
            }
            return false;
        }
        if (item.rowSpan < 1) {
            item.rowSpan = 1;
        }
        if (item.colSpan < 1) {
            item.colSpan = 1;
        }
        maxRow = qMax(maxRow, item.row + item.rowSpan - 1);
        maxCol = qMax(maxCol, item.col + item.colSpan - 1);

        // Optional per-item dwell / progress overrides (item > layout > global).
        if (io.contains(QStringLiteral("dwell"))) {
            parseDwellObject(io.value(QStringLiteral("dwell")).toObject(), item.dwell);
        }

        item.unbounded = io.value(QStringLiteral("unbounded")).toBool(false)
                         || io.value(QStringLiteral("role")).toString().toLower()
                                == QLatin1String("unbounded");
        item.actionLoop = io.value(QStringLiteral("actionLoop")).toBool(false)
                          || io.value(QStringLiteral("loop")).toBool(false);
        if (io.contains(QStringLiteral("dwellRegion"))) {
            const QJsonObject dr = io.value(QStringLiteral("dwellRegion")).toObject();
            item.hasDwellRegion = true;
            parseDim(dr, QStringLiteral("x"), item.dwellRegion.x);
            parseDim(dr, QStringLiteral("y"), item.dwellRegion.y);
            parseDim(dr, QStringLiteral("width"), item.dwellRegion.width);
            parseDim(dr, QStringLiteral("height"), item.dwellRegion.height);
            // Legacy: bare x/y as pixels when written as numbers without % intent
            // — new default is percent; existing layouts used board-local pixel x/y.
            // If only "x"/"y" numbers and no xPx, treat as pixels when screenAnchor is none
            // for back-compat with board-local regions (old schema).
            item.dwellRegion.marginPx = dr.value(QStringLiteral("marginPx")).toInt(4);
            // Legacy width/height absolute fields → DimSpec only.
            if (!item.dwellRegion.width.isSet()) {
                const int w = dr.value(QStringLiteral("widthPx"))
                                  .toInt(dr.value(QStringLiteral("width")).toInt(80));
                item.dwellRegion.width = DimSpec::pixels(w);
            }
            if (!item.dwellRegion.height.isSet()) {
                const int h = dr.value(QStringLiteral("heightPx"))
                                  .toInt(dr.value(QStringLiteral("height")).toInt(80));
                item.dwellRegion.height = DimSpec::pixels(h);
            }
            // If x/y were only legacy doubles without unit, parseDim set percent —
            // for board-local (no anchor) old files used pixel coords: detect legacy.
            item.dwellRegion.screenAnchor =
                parseScreenAnchor(dr.value(QStringLiteral("screenAnchor")).toString());
            if (dr.contains(QStringLiteral("boundsMode")) || dr.contains(QStringLiteral("bounds"))) {
                item.dwellRegion.hasBoundsMode = true;
                const QString raw = dr.contains(QStringLiteral("boundsMode"))
                                        ? dr.value(QStringLiteral("boundsMode")).toString()
                                        : dr.value(QStringLiteral("bounds")).toString();
                item.dwellRegion.boundsMode =
                    parseBoundsMode(raw, layout.effectiveBoundsMode());
            }
            if (!item.dwellRegion.usesScreenAnchor()) {
                // Board-local legacy: bare numeric x/y/width/height are pixels
                // (percent requires "N%" string or *Px explicit for new schema).
                auto forcePxIfBareNumber = [&](const QString& key, const QString& pxKey,
                                               DimSpec& dim) {
                    if (dr.contains(pxKey)) {
                        return;
                    }
                    if (!dr.contains(key)) {
                        return;
                    }
                    const QJsonValue v = dr.value(key);
                    if (v.isDouble()) {
                        dim = DimSpec::pixels(v.toDouble(0));
                    }
                };
                forcePxIfBareNumber(QStringLiteral("x"), QStringLiteral("xPx"),
                                    item.dwellRegion.x);
                forcePxIfBareNumber(QStringLiteral("y"), QStringLiteral("yPx"),
                                    item.dwellRegion.y);
                forcePxIfBareNumber(QStringLiteral("width"), QStringLiteral("widthPx"),
                                    item.dwellRegion.width);
                forcePxIfBareNumber(QStringLiteral("height"), QStringLiteral("heightPx"),
                                    item.dwellRegion.height);
            }
            if (item.dwellRegion.usesScreenAnchor()) {
                item.unbounded = true;
            }
        }

        if (io.contains(QStringLiteral("actions"))) {
            if (!parseActionList(io.value(QStringLiteral("actions")), item.actions, error)) {
                return false;
            }
            if (!item.actions.isEmpty()) {
                item.action = item.actions.first();
            }
        }
        if (io.contains(QStringLiteral("action"))) {
            if (!parseAction(io.value(QStringLiteral("action")).toObject(), item.action, error)) {
                return false;
            }
            if (item.actions.isEmpty()) {
                item.actions.push_back(item.action);
            }
        }

        auto isDwellSuspendCmd = [](const LayoutAction& a) {
            return a.type == LayoutAction::Type::Command
                   && (a.name == QLatin1String("toggleDwellSuspend")
                       || a.name == QLatin1String("suspendDwell")
                       || a.name == QLatin1String("resumeDwell"));
        };
        if (isDwellSuspendCmd(item.action)) {
            item.dwellExempt = true;
        }
        for (const LayoutAction& a : item.actions) {
            if (isDwellSuspendCmd(a)) {
                item.dwellExempt = true;
                break;
            }
        }

        if (io.contains(QStringLiteral("style"))) {
            parseChromeStyle(io.value(QStringLiteral("style")).toObject(), item.style);
        } else {
            parseChromeStyle(io, item.style);
        }

        layout.items.push_back(std::move(item));
    }

    // Expand default / undersized grid so cells fit.
    if (!root.contains(QStringLiteral("grid"))) {
        layout.grid.columns = qMax(1, maxCol + 1);
        layout.grid.rows = qMax(1, maxRow + 1);
    } else {
        layout.grid.columns = qMax(layout.grid.columns, maxCol + 1);
        layout.grid.rows = qMax(layout.grid.rows, maxRow + 1);
    }

    out = std::move(layout);
    GAZER_DEBUG << "Loaded layout" << out.id << "items:" << out.items.size();
    return true;
}

} // namespace gazer
