#include "app/SettingsUi.h"
#include "app/CommandRegistry.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"
#include "assist/LookToMap.h"
#include "assist/LookToMaps.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"

#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::adoptCallerBoard;
using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsPageBuild::makeNested;
using SettingsUiInternal::kLiveLookToMap;

namespace {

void addSettingsChrome(PageDocument& doc)
{
    auto put = [&](const char* id, const char* bg, const PageBox& radius) {
        PageChrome c;
        c.background.token = QLatin1String(bg);
        c.thickness = PageBox::all(0.0);
        c.radius = radius;
        doc.styles.insert(QLatin1String(id), c);
    };
    put("group", "bg95", PageBox::all(16.0));
    put("row", "transparent", PageBox::all(0.0));
    put("plain", "transparent", PageBox::all(0.0));
    put("join", "transparent", PageBox::all(0.0));
    put("joinLeft", "transparent", PageBox::of(10.0, 0.0, 0.0, 10.0));
    put("joinRight", "transparent", PageBox::of(0.0, 10.0, 10.0, 0.0));
}

const char* segmentStyle(int index, int count)
{
    if (count > 1 && index == 0) {
        return "joinLeft";
    }
    if (count > 1 && index == count - 1) {
        return "joinRight";
    }
    return "join";
}

PageGrid stepper(const QString& id, const QString& value, const QString& caption,
                 const QString& decCmd, const QString& incCmd, const QString& editCmd)
{
    PageGrid g = makeNested(id, 0, 0, 1, 9, 0);
    g.styleId = QStringLiteral("row");
    auto piece = [&](const QString& suffix, const QString& lab, int col, int span,
                     const QString& cmd, const QString& role, const QString& cap,
                     const QString& icon, const char* style) {
        PageCell c = cell(id + suffix, lab, 0, col, cmd, {}, span, role, cap, icon);
        c.styleId = QLatin1String(style);
        g.cells.push_back(c);
    };
    piece(QStringLiteral("_dec"), QStringLiteral("−"), 0, 2, decCmd, {}, {}, {}, "joinLeft");
    piece(QStringLiteral("_val"), value, 2, 3, {}, QStringLiteral("value"), caption, {}, "join");
    piece(QStringLiteral("_inc"), QStringLiteral("+"), 5, 2, incCmd, {}, {}, {}, "join");
    piece(QStringLiteral("_edit"), QStringLiteral("Edit…"), 7, 2, editCmd, {}, {},
          QStringLiteral("editSquare"), "joinRight");
    return g;
}

PageGrid labeled(const QString& id, const QString& label, const QString& caption, PageGrid control)
{
    PageGrid g = makeNested(id, 0, 0, 1, 2, 6);
    g.styleId = QStringLiteral("row");
    PageCell lab = cell(id + QStringLiteral("_l"), label, 0, 0, {}, {}, 1, QStringLiteral("label"),
                        caption);
    lab.styleId = QStringLiteral("plain");
    g.cells.push_back(lab);
    control.row = 0;
    control.col = 1;
    g.subGrids.push_back(std::move(control));
    return g;
}

PageGrid oneToggle(const QString& id, const QString& label, const QString& command,
                   const QString& active)
{
    PageGrid g = makeNested(id, 0, 0, 1, 1, 0);
    g.styleId = QStringLiteral("row");
    PageCell t = cell(id + QStringLiteral("_t"), label, 0, 0, command, {}, 1, QStringLiteral("toggle"));
    t.activeState = active;
    t.styleId = QStringLiteral("join");
    g.cells.push_back(t);
    return g;
}

PageGrid choiceRow(const QString& id, const char* const* ids, const char* const* labs, int n,
                   const QString& cmdPrefix, const QString& activePrefix)
{
    PageGrid g = makeNested(id, 0, 0, 1, n, 0);
    g.styleId = QStringLiteral("row");
    for (int i = 0; i < n; ++i) {
        const QString stem = QLatin1String(ids[i]);
        PageCell c = cell(id + QStringLiteral("_") + stem, QLatin1String(labs[i]), 0, i,
                          cmdPrefix + stem, {}, 1, QStringLiteral("choice"));
        c.activeState = activePrefix + stem;
        c.styleId = QLatin1String(segmentStyle(i, n));
        g.cells.push_back(c);
    }
    return g;
}

PageGrid partToggles(const QString& id, bool border)
{
    PageGrid g = makeNested(id, 0, 0, 1, kLookToPartCount, 0);
    g.styleId = QStringLiteral("row");
    int i = 0;
    for (const LookToPartSpec& spec : kLookToParts) {
        const QString state = QLatin1String(border ? spec.borderState : spec.fillState);
        PageCell c = cell(id + QLatin1Char('_') + QLatin1String(spec.id), QLatin1String(spec.label),
                          0, i, state + QStringLiteral(".toggle"), {}, 1, QStringLiteral("toggle"));
        c.activeState = state;
        c.styleId = QLatin1String(segmentStyle(i, kLookToPartCount));
        g.cells.push_back(c);
        ++i;
    }
    return g;
}

PageGrid groupBox(const QString& id, const QString& title, QVector<PageGrid> rows)
{
    PageGrid g = makeNested(id, 0, 0, rows.size() + 1, 1, 6);
    g.styleId = QStringLiteral("group");
    g.marginPx = 6;
    QVector<double> weights;
    weights.push_back(1.0);
    for (int i = 0; i < rows.size(); ++i) {
        weights.push_back(2.0);
    }
    g.rowTracks = starTracks(weights);
    PageCell h = cell(id + QStringLiteral("_h"), title, 0, 0, {}, {}, 1, QStringLiteral("label"));
    h.textStyle = QStringLiteral("section");
    h.styleId = QStringLiteral("plain");
    g.cells.push_back(h);
    for (int i = 0; i < rows.size(); ++i) {
        rows[i].row = i + 1;
        rows[i].col = 0;
        g.subGrids.push_back(std::move(rows[i]));
    }
    return g;
}

PageGrid columnOf(const QString& id, int col, const QVector<double>& weights, QVector<PageGrid> groups)
{
    PageGrid g = makeNested(id, 0, col, groups.size(), 1, 6);
    g.rowTracks = starTracks(weights);
    for (int i = 0; i < groups.size(); ++i) {
        groups[i].row = i;
        groups[i].col = 0;
        g.subGrids.push_back(std::move(groups[i]));
    }
    return g;
}

} // namespace

LookToMapSettings* SettingsUi::lookToDraft()
{
    return &m_settings.lookToMap(m_lookToDest);
}

const LookToMapSettings* SettingsUi::lookToDraft() const
{
    return &m_settings.lookToMap(m_lookToDest);
}

void SettingsUi::commitLookToDraft(const LookToMapSettings& c, bool persist)
{
    m_settings.lookToMap(m_lookToDest) = c;
    clampLookToMapSettings(m_lookToDest, m_settings.lookToMap(m_lookToDest));
    if (m_lookTo) {
        m_lookTo->applyConfig(m_lookToDest, m_settings.lookToMap(m_lookToDest));
        m_lookTo->setHighlightRing(m_lookToDest, m_lookToRing);
    }
    if (persist) {
        apply(true);
    }
}

void SettingsUi::syncLookToPreview()
{
    if (!m_lookTo || !m_lookToMap.active) {
        return;
    }
    m_lookTo->setPreview(m_lookToDest, m_lookToPreview);
    if (m_lookToPreview) {
        m_lookTo->setHighlightRing(m_lookToDest, m_lookToRing);
    }
}

void SettingsUi::closeLookToEditor()
{
    if (m_lookTo) {
        m_lookTo->clearPreview();
    }
    closeLive(m_lookToMap);
}

bool SettingsUi::openLookToEditor(LookToDest dest, QString* error)
{
    m_lookToDest = dest;
    m_lookToRing = LookToRing::Deadzone;
    if (!presentLive(m_lookToMap, QLatin1String(kLiveLookToMap), buildLookToEditor(), error)) {
        return false;
    }
    syncLookToPreview();
    m_pages.refreshActive();
    return true;
}

void SettingsUi::refreshLookToEditor()
{
    if (!m_lookToMap.active) {
        return;
    }
    QString err;
    (void)m_pages.attachDocument(buildLookToEditor(), &err, false, false);
    syncLookToPreview();
    m_pages.refreshActive();
}

void SettingsUi::lookToNudge(LookToRing ring, int dir)
{
    LookToMapSettings* c = lookToDraft();
    if (!c) {
        return;
    }
    m_lookToRing = ring;
    const int step = dir > 0 ? 10 : -10;
    switch (ring) {
    case LookToRing::Deadzone:
        c->deadzonePx += step;
        break;
    case LookToRing::Max:
        c->maxPx += step;
        break;
    case LookToRing::Outer:
        c->outerDeadzonePx += step;
        break;
    case LookToRing::None:
        break;
    }
    commitLookToDraft(*c);
    refreshLookToEditor();
}

void SettingsUi::lookToNudgeSpeed(int dir)
{
    LookToMapSettings* c = lookToDraft();
    if (!c) {
        return;
    }
    c->maxSpeed = nudgeLookToSpeed(m_lookToDest, c->maxSpeed, dir);
    commitLookToDraft(*c);
    refreshLookToEditor();
}

void SettingsUi::lookToNudgeAccel(int dir)
{
    LookToMapSettings* c = lookToDraft();
    if (!c) {
        return;
    }
    c->accelPerSec = qBound(kLtsAccelMin, c->accelPerSec + dir * 0.5, kLtsAccelMax);
    commitLookToDraft(*c);
    refreshLookToEditor();
}

void SettingsUi::lookToNudgeCenterDwell(int dir)
{
    LookToMapSettings* c = lookToDraft();
    if (!c) {
        return;
    }
    c->centerDwellMs = qBound(200, c->centerDwellMs + dir * 50, 2500);
    commitLookToDraft(*c);
    refreshLookToEditor();
}

bool SettingsUi::lookToEditField(const QString& field, QString* error)
{
    const LookToMapSettings* c = lookToDraft();
    if (!c) {
        if (error) {
            *error = QStringLiteral("No map");
        }
        return false;
    }
    m_lookToNumpadField = field;
    m_numpadReturn = NumpadReturn::LookTo;
    m_numpadKey = field;
    QString seed;
    QString title;
    QString hint;
    if (field == QLatin1String("deadzone")) {
        m_lookToRing = LookToRing::Deadzone;
        seed = QString::number(c->deadzonePx);
        title = QStringLiteral("Deadzone");
        hint = QStringLiteral("Inner no-output radius (px).");
    } else if (field == QLatin1String("max")) {
        m_lookToRing = LookToRing::Max;
        seed = QString::number(c->maxPx);
        title = QStringLiteral("Max");
        hint = QStringLiteral("Radius where output reaches 100% (px).");
    } else if (field == QLatin1String("outer")) {
        m_lookToRing = LookToRing::Outer;
        seed = QString::number(c->outerDeadzonePx);
        title = QStringLiteral("Outer deadzone");
        hint = QStringLiteral("Outer deadzone radius (px).");
    } else if (field == QLatin1String("centerDwell")) {
        seed = QString::number(c->centerDwellMs);
        title = QStringLiteral("Hub dwell");
        hint = QStringLiteral("Dwell the hub to open the pie (ms).");
    } else if (field == QLatin1String("accel")) {
        seed = QString::number(c->accelPerSec, 'f', 1);
        title = QStringLiteral("Accel / s");
        hint = QStringLiteral("Speed growth per second while engaged.");
    } else if (field == QLatin1String("speed")) {
        seed = QString::number(c->maxSpeed, 'f', 2);
        title = QStringLiteral("Speed");
        hint = QStringLiteral("Peak output at 100%.");
    } else {
        if (error) {
            *error = QStringLiteral("Unknown field");
        }
        return false;
    }
    m_numpadTitle = title;
    m_numpadHint = hint;
    m_numpadResetSeed = seed;
    m_numpadBuffer = seed;
    return presentNumpad(error);
}

void SettingsUi::applyLookToNumpad(double v)
{
    LookToMapSettings* c = lookToDraft();
    if (!c) {
        return;
    }
    const QString f = m_lookToNumpadField;
    if (f == QLatin1String("deadzone")) {
        c->deadzonePx = int(qRound(v));
        m_lookToRing = LookToRing::Deadzone;
    } else if (f == QLatin1String("max")) {
        c->maxPx = int(qRound(v));
        m_lookToRing = LookToRing::Max;
    } else if (f == QLatin1String("outer")) {
        c->outerDeadzonePx = int(qRound(v));
        m_lookToRing = LookToRing::Outer;
    } else if (f == QLatin1String("centerDwell")) {
        c->centerDwellMs = int(qRound(v));
    } else if (f == QLatin1String("accel")) {
        c->accelPerSec = v;
    } else if (f == QLatin1String("speed")) {
        c->maxSpeed = v;
    }
    commitLookToDraft(*c);
    refreshLookToEditor();
}

PageDocument SettingsUi::buildLookToEditor() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveLookToMap);
    doc.name = QLatin1String(lookToDestLabel(m_lookToDest));
    const ThemeColors theme = m_settings.resolvedTheme();
    initGrid(doc, 16, 2, 1080, 900, 6, 12, theme);
    addSettingsChrome(doc);
    PageGrid& grid = doc.grids[0];
    adoptCallerBoard(grid, m_pages.pageBehind(QLatin1String(kLiveLookToMap)));
    grid.rows = 2;
    grid.columns = 16;
    grid.rowTracks = starTracks({1.0, 8.0});
    const EditorSwatch sw = editorSwatch();
    const LookToMapSettings* m = lookToDraft();
    LookToMapSettings fallback = defaultLookToMapSettings(m_lookToDest);
    if (!m) {
        m = &fallback;
    }
    const QString enableCmd = QLatin1String(lookToDestCommand(m_lookToDest));

    PageCell title =
        cell(QStringLiteral("title"), QLatin1String(lookToDestLabel(m_lookToDest)), 0, 0, {},
             QColor(), 14, QStringLiteral("label"),
             QStringLiteral("Preview draws rings at the origin. The live tool stretches the inner deadzone."));
    title.textStyle = QStringLiteral("title");
    title.styleId = QStringLiteral("plain");
    grid.cells.push_back(title);
    grid.cells.push_back(cell(QStringLiteral("done"), QStringLiteral("Done"), 0, 14,
                              QStringLiteral("lookTo.map.done"), sw.save, 2, {}, {},
                              QStringLiteral("check")));

    PageGrid body = makeNested(QStringLiteral("body"), 1, 0, 1, 2, 16);
    body.colSpan = 16;
    const char* modeIds[] = {"vertical", "horizontal", "both"};
    const char* modeLabs[] = {"V", "H", "Both"};
    body.subGrids.push_back(columnOf(
        QStringLiteral("left"), 0, {9.0, 7.0},
        {groupBox(QStringLiteral("sec_map"), QStringLiteral("Map"),
                  {labeled(QStringLiteral("en"), QStringLiteral("Enable"),
                           QStringLiteral("Place origin, then look to drive this map."),
                           oneToggle(QStringLiteral("en_t"), QStringLiteral("On"), enableCmd,
                                     enableCmd)),
                   labeled(QStringLiteral("prev"), QStringLiteral("Preview"),
                           QStringLiteral("Draw analog rings at the origin."),
                           oneToggle(QStringLiteral("prev_t"), QStringLiteral("On"),
                                     QStringLiteral("lookTo.map.preview.toggle"),
                                     QStringLiteral("lookTo.map.preview"))),
                   labeled(QStringLiteral("hub"), QStringLiteral("Hub"),
                           QStringLiteral("Center pie: speed, place, direction, close."),
                           oneToggle(QStringLiteral("hub_t"), QStringLiteral("On"),
                                     QStringLiteral("lookTo.map.hub.toggle"),
                                     QStringLiteral("lookTo.map.hub"))),
                   labeled(QStringLiteral("hubd"), QStringLiteral("Hub dwell"),
                           QStringLiteral("Dwell the hub to open the pie."),
                           stepper(QStringLiteral("hubd_s"),
                                   QStringLiteral("%1 ms").arg(m->centerDwellMs),
                                   QStringLiteral("ms"),
                                   QStringLiteral("lookTo.map.nudge.centerDwell.dec"),
                                   QStringLiteral("lookTo.map.nudge.centerDwell.inc"),
                                   QStringLiteral("lookTo.map.edit.centerDwell")))}),
         groupBox(QStringLiteral("sec_out"), QStringLiteral("Output"),
                  {labeled(QStringLiteral("spd"), QStringLiteral("Speed"),
                           QStringLiteral("Peak at 100%."),
                           stepper(QStringLiteral("spd_s"),
                                   lookToSpeedText(m_lookToDest, m->maxSpeed), {},
                                   QStringLiteral("lookTo.map.nudge.speed.dec"),
                                   QStringLiteral("lookTo.map.nudge.speed.inc"),
                                   QStringLiteral("lookTo.map.edit.speed"))),
                   labeled(QStringLiteral("dir"), QStringLiteral("Direction"),
                           QStringLiteral("Lock to one axis, or both."),
                           choiceRow(QStringLiteral("mode"), modeIds, modeLabs, 3,
                                     QStringLiteral("lookTo.map.mode."),
                                     QStringLiteral("lookTo.map.mode."))),
                   labeled(QStringLiteral("accel"), QStringLiteral("Accel / s"),
                           QStringLiteral("Grows while that axis is engaged."),
                           stepper(QStringLiteral("accel_s"),
                                   QStringLiteral("%1 /s").arg(m->accelPerSec, 0, 'f', 1),
                                   QStringLiteral("/s"),
                                   QStringLiteral("lookTo.map.nudge.accel.dec"),
                                   QStringLiteral("lookTo.map.nudge.accel.inc"),
                                   QStringLiteral("lookTo.map.edit.accel")))})}));
    body.subGrids.push_back(columnOf(
        QStringLiteral("right"), 1, {9.0, 5.0},
        {groupBox(QStringLiteral("sec_rings"), QStringLiteral("Rings"),
                  {labeled(QStringLiteral("dz"), QStringLiteral("Deadzone"),
                           QStringLiteral("Inner circle. Output is 0."),
                           stepper(QStringLiteral("dz_s"), QStringLiteral("%1 px").arg(m->deadzonePx),
                                   QStringLiteral("px"),
                                   QStringLiteral("lookTo.map.nudge.deadzone.dec"),
                                   QStringLiteral("lookTo.map.nudge.deadzone.inc"),
                                   QStringLiteral("lookTo.map.edit.deadzone"))),
                   labeled(QStringLiteral("max"), QStringLiteral("Max"),
                           QStringLiteral("Output reaches 100% here and stays there outside it."),
                           stepper(QStringLiteral("max_s"), QStringLiteral("%1 px").arg(m->maxPx),
                                   QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.max.dec"),
                                   QStringLiteral("lookTo.map.nudge.max.inc"),
                                   QStringLiteral("lookTo.map.edit.max"))),
                   labeled(QStringLiteral("outer"), QStringLiteral("Outer deadzone"),
                           QStringLiteral("Past this edge, output drops to 0."),
                           oneToggle(QStringLiteral("outer_t"), QStringLiteral("On"),
                                     QStringLiteral("lookTo.map.outerDeadzone.toggle"),
                                     QStringLiteral("lookTo.map.outerDeadzone"))),
                   labeled(QStringLiteral("outerr"), QStringLiteral("Outer radius"),
                           QStringLiteral("Outer deadzone radius."),
                           stepper(QStringLiteral("outer_s"),
                                   QStringLiteral("%1 px").arg(m->outerDeadzonePx),
                                   QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.outer.dec"),
                                   QStringLiteral("lookTo.map.nudge.outer.inc"),
                                   QStringLiteral("lookTo.map.edit.outer")))}),
         groupBox(QStringLiteral("sec_style"), QStringLiteral("Style"),
                  {labeled(QStringLiteral("border"), QStringLiteral("Draw border"),
                           QStringLiteral("Outline pause, inner, max, and outer."),
                           partToggles(QStringLiteral("border"), true)),
                   labeled(QStringLiteral("fill"), QStringLiteral("Fill background"),
                           QStringLiteral("Fill pause, inner, max, and outer."),
                           partToggles(QStringLiteral("fill"), false))})}));
    grid.subGrids.push_back(std::move(body));
    return doc;
}

void SettingsUi::registerLookToCommands()
{
    m_commands.registerPrefix(
        QStringLiteral("lookTo.edit."),
        [this](const CommandRegistry::Invocation& inv, QString* e) {
            bool ok = false;
            const LookToDest dest =
                lookToDestFromId(inv.name.mid(int(QLatin1String("lookTo.edit.").size())), &ok);
            if (!ok) {
                return false;
            }
            return openLookToEditor(dest, e);
        });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.done"), [this](QString*) {
        closeLookToEditor();
        apply(true);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.preview.toggle"), [this](QString*) {
        m_lookToPreview = !m_lookToPreview;
        syncLookToPreview();
        refreshLookToEditor();
        notifyStatus(m_lookToPreview ? QStringLiteral("Look-to preview on")
                                     : QStringLiteral("Look-to preview off"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.hub.toggle"), [this](QString*) {
        if (LookToMapSettings* c = lookToDraft()) {
            c->hubEnabled = !c->hubEnabled;
            commitLookToDraft(*c);
            refreshLookToEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.outerDeadzone.toggle"), [this](QString*) {
        if (LookToMapSettings* c = lookToDraft()) {
            c->outerDeadzoneEnabled = !c->outerDeadzoneEnabled;
            m_lookToRing = LookToRing::Outer;
            commitLookToDraft(*c);
            refreshLookToEditor();
        }
        return true;
    });

    auto nudgeRing = [this](LookToRing ring, int dir) {
        return [this, ring, dir](QString*) {
            lookToNudge(ring, dir);
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.deadzone.dec"),
                               nudgeRing(LookToRing::Deadzone, -1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.deadzone.inc"),
                               nudgeRing(LookToRing::Deadzone, +1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.max.dec"),
                               nudgeRing(LookToRing::Max, -1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.max.inc"),
                               nudgeRing(LookToRing::Max, +1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.outer.dec"),
                               nudgeRing(LookToRing::Outer, -1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.outer.inc"),
                               nudgeRing(LookToRing::Outer, +1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.speed.dec"), [this](QString*) {
        lookToNudgeSpeed(-1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.speed.inc"), [this](QString*) {
        lookToNudgeSpeed(+1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.accel.dec"), [this](QString*) {
        lookToNudgeAccel(-1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.accel.inc"), [this](QString*) {
        lookToNudgeAccel(+1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.centerDwell.dec"), [this](QString*) {
        lookToNudgeCenterDwell(-1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.centerDwell.inc"), [this](QString*) {
        lookToNudgeCenterDwell(+1);
        return true;
    });

    m_commands.registerPrefix(
        QStringLiteral("lookTo.map.edit."),
        [this](const CommandRegistry::Invocation& inv, QString* e) {
            return lookToEditField(inv.name.mid(int(QLatin1String("lookTo.map.edit.").size())), e);
        });

    m_commands.registerBuiltin(QStringLiteral("lookTo.map.mode.vertical"), [this](QString*) {
        if (LookToMapSettings* c = lookToDraft()) {
            c->axisMode = LtsScrollMode::Vertical;
            commitLookToDraft(*c);
            refreshLookToEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.mode.horizontal"), [this](QString*) {
        if (LookToMapSettings* c = lookToDraft()) {
            c->axisMode = LtsScrollMode::Horizontal;
            commitLookToDraft(*c);
            refreshLookToEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.mode.both"), [this](QString*) {
        if (LookToMapSettings* c = lookToDraft()) {
            c->axisMode = LtsScrollMode::Both;
            commitLookToDraft(*c);
            refreshLookToEditor();
        }
        return true;
    });
    auto togglePart = [this](LookToPart part, bool border) {
        return [this, part, border](QString*) {
            if (LookToMapSettings* c = lookToDraft()) {
                bool& flag = border ? c->chrome(part).border : c->chrome(part).fill;
                flag = !flag;
                commitLookToDraft(*c);
                refreshLookToEditor();
            }
            return true;
        };
    };
    for (const LookToPartSpec& spec : kLookToParts) {
        m_commands.registerBuiltin(QLatin1String(spec.borderState) + QStringLiteral(".toggle"),
                                   togglePart(spec.part, true));
        m_commands.registerBuiltin(QLatin1String(spec.fillState) + QStringLiteral(".toggle"),
                                   togglePart(spec.part, false));
    }
}

} // namespace gazer
