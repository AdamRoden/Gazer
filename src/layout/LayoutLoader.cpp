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

    const QJsonObject grid = root.value(QStringLiteral("grid")).toObject();
    layout.grid.columns = grid.value(QStringLiteral("columns")).toInt(1);
    layout.grid.rows = grid.value(QStringLiteral("rows")).toInt(1);
    layout.grid.gapPx = grid.value(QStringLiteral("gapPx")).toInt(8);
    layout.grid.marginPx = grid.value(QStringLiteral("marginPx")).toInt(16);
    layout.grid.unitRows = grid.value(QStringLiteral("unitRows")).toBool(false);

    if (layout.grid.columns < 1 || layout.grid.rows < 1) {
        if (error) {
            *error = QStringLiteral("Layout grid columns/rows must be >= 1");
        }
        return false;
    }

    // Optional layout-level dwell override (overrides AppSettings when present).
    if (root.contains(QStringLiteral("dwell"))) {
        parseDwellObject(root.value(QStringLiteral("dwell")).toObject(), layout.dwell);
    }

    // Optional window: { "anchor", "widthPx", "heightPx", "marginPx", "hidden": true }
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
        layout.placement.widthPx = win.value(QStringLiteral("widthPx")).toInt(0);
        layout.placement.heightPx = win.value(QStringLiteral("heightPx")).toInt(0);
        layout.placement.marginPx = win.value(QStringLiteral("marginPx")).toInt(8);
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    layout.items.reserve(items.size());
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

        // Optional per-item dwell / progress overrides (item > layout > global).
        if (io.contains(QStringLiteral("dwell"))) {
            parseDwellObject(io.value(QStringLiteral("dwell")).toObject(), item.dwell);
        }

        item.unbounded = io.value(QStringLiteral("unbounded")).toBool(false)
                         || io.value(QStringLiteral("role")).toString().toLower()
                                == QLatin1String("unbounded");
        if (io.contains(QStringLiteral("dwellRegion"))) {
            const QJsonObject dr = io.value(QStringLiteral("dwellRegion")).toObject();
            item.hasDwellRegion = true;
            item.dwellRegion.x = dr.value(QStringLiteral("x")).toDouble(0);
            item.dwellRegion.y = dr.value(QStringLiteral("y")).toDouble(0);
            item.dwellRegion.widthPx = dr.value(QStringLiteral("widthPx")).toInt(
                dr.value(QStringLiteral("width")).toInt(80));
            item.dwellRegion.heightPx = dr.value(QStringLiteral("heightPx")).toInt(
                dr.value(QStringLiteral("height")).toInt(80));
            item.dwellRegion.marginPx = dr.value(QStringLiteral("marginPx")).toInt(4);
            const QString anchor =
                dr.value(QStringLiteral("screenAnchor")).toString().toLower();
            if (anchor == QLatin1String("top")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Top;
            } else if (anchor == QLatin1String("bottom")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Bottom;
            } else if (anchor == QLatin1String("left")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Left;
            } else if (anchor == QLatin1String("right")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Right;
            } else if (anchor == QLatin1String("topleft")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::TopLeft;
            } else if (anchor == QLatin1String("topright")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::TopRight;
            } else if (anchor == QLatin1String("bottomleft")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::BottomLeft;
            } else if (anchor == QLatin1String("bottomright")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::BottomRight;
            } else if (anchor == QLatin1String("topcenter")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::TopCenter;
            } else if (anchor == QLatin1String("bottomcenter")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::BottomCenter;
            } else if (anchor == QLatin1String("leftcenter")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::LeftCenter;
            } else if (anchor == QLatin1String("rightcenter")) {
                item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::RightCenter;
            }
            if (item.dwellRegion.usesScreenAnchor()) {
                item.unbounded = true;
            }
        }

        if (io.contains(QStringLiteral("action"))) {
            if (!parseAction(io.value(QStringLiteral("action")).toObject(), item.action, error)) {
                return false;
            }
            // Convenience: suspend/resume controls stay dwellable under global suspend.
            if (item.action.type == LayoutAction::Type::Command
                && (item.action.name == QLatin1String("toggleDwellSuspend")
                    || item.action.name == QLatin1String("suspendDwell")
                    || item.action.name == QLatin1String("resumeDwell"))) {
                item.dwellExempt = true;
            }
        }

        if (io.contains(QStringLiteral("style"))) {
            const QJsonObject st = io.value(QStringLiteral("style")).toObject();
            item.style.background = parseColor(st.value(QStringLiteral("background")));
            item.style.foreground = parseColor(st.value(QStringLiteral("foreground")));
        }

        layout.items.push_back(std::move(item));
    }

    out = std::move(layout);
    GAZER_DEBUG << "Loaded layout" << out.id << "items:" << out.items.size();
    return true;
}

} // namespace gazer
