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

void joinEnds(PageCell& c, int index, int count)
{
    c.style.thickness = PageBox::all(0.0);
    if (count <= 1) {
        c.style.radius = PageBox::all(10.0);
        return;
    }
    if (index == 0) {
        c.style.radius = PageBox::of(10.0, 0.0, 0.0, 10.0);
    } else if (index == count - 1) {
        c.style.radius = PageBox::of(0.0, 10.0, 10.0, 0.0);
    } else {
        c.style.radius = PageBox::all(0.0);
    }
}

PageCell sectionLabel(const QString& id, const QString& text, int row, int col, int span = 1)
{
    PageCell c = cell(id, text, row, col, {}, QColor(), span, QStringLiteral("label"));
    c.textStyle = QStringLiteral("section");
    return c;
}

PageGrid stepper(const QString& id, const QString& value, const QString& caption,
                 const QString& decCmd, const QString& incCmd, const QString& editCmd,
                 const SettingsUi::EditorSwatch& sw)
{
    PageGrid g = makeNested(id, 0, 0, 1, 9, 0);
    PageCell dec = cell(id + QStringLiteral("_dec"), QStringLiteral("−"), 0, 0, decCmd, sw.nudge, 2);
    joinEnds(dec, 0, 4);
    g.cells.push_back(dec);
    PageCell val = cell(id + QStringLiteral("_val"), value, 0, 2, {}, sw.value, 3,
                        QStringLiteral("value"), caption);
    joinEnds(val, 1, 4);
    g.cells.push_back(val);
    PageCell inc = cell(id + QStringLiteral("_inc"), QStringLiteral("+"), 0, 5, incCmd, sw.nudge, 2);
    joinEnds(inc, 2, 4);
    g.cells.push_back(inc);
    PageCell edit = cell(id + QStringLiteral("_edit"), QStringLiteral("Edit…"), 0, 7, editCmd, sw.edit,
                         2, {}, {}, QStringLiteral("editSquare"));
    joinEnds(edit, 3, 4);
    g.cells.push_back(edit);
    return g;
}

PageGrid labeled(const QString& id, int row, const QString& label, const QString& caption,
                 PageGrid control)
{
    PageGrid g = makeNested(id, row, 0, 1, 2, 6);
    g.columnTracks = starTracks({1.2, 2.0});
    PageCell lab = cell(id + QStringLiteral("_l"), label, 0, 0, {}, QColor(), 1,
                        QStringLiteral("label"), caption);
    g.cells.push_back(lab);
    control.row = 0;
    control.col = 1;
    g.subGrids.push_back(std::move(control));
    return g;
}

PageGrid labeledToggle(const QString& id, int row, const QString& label, const QString& caption,
                       const QString& toggleLabel, const QString& command, const QString& active)
{
    PageGrid g = makeNested(id, row, 0, 1, 2, 6);
    g.columnTracks = starTracks({1.2, 2.0});
    g.cells.push_back(cell(id + QStringLiteral("_l"), label, 0, 0, {}, QColor(), 1,
                           QStringLiteral("label"), caption));
    PageCell t = cell(id + QStringLiteral("_t"), toggleLabel, 0, 1, command, QColor(), 1,
                      QStringLiteral("toggle"));
    t.activeState = active;
    g.cells.push_back(t);
    return g;
}

PageGrid choiceRow(const QString& id, const char* const* ids, const char* const* labs, int n,
                   const QString& cmdPrefix, const QString& activePrefix)
{
    PageGrid g = makeNested(id, 0, 0, 1, n, 0);
    for (int i = 0; i < n; ++i) {
        const QString stem = QLatin1String(ids[i]);
        PageCell c = cell(id + QStringLiteral("_") + stem, QLatin1String(labs[i]), 0, i,
                          cmdPrefix + stem, QColor(), 1, QStringLiteral("choice"));
        c.activeState = activePrefix + stem;
        joinEnds(c, i, n);
        g.cells.push_back(c);
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
    case LookToRing::Ramp:
        c->rampEndPx += step;
        break;
    case LookToRing::Full:
        c->fullOuterPx += step;
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
    } else if (field == QLatin1String("ramp")) {
        m_lookToRing = LookToRing::Ramp;
        seed = QString::number(c->rampEndPx);
        title = QStringLiteral("Ramp end");
        hint = QStringLiteral("Radius where speed reaches 100% (px).");
    } else if (field == QLatin1String("full")) {
        m_lookToRing = LookToRing::Full;
        seed = QString::number(c->fullOuterPx);
        title = QStringLiteral("100% outer");
        hint = QStringLiteral("Outer edge of the 100% ring (px).");
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
    } else if (f == QLatin1String("ramp")) {
        c->rampEndPx = int(qRound(v));
        m_lookToRing = LookToRing::Ramp;
    } else if (f == QLatin1String("full")) {
        c->fullOuterPx = int(qRound(v));
        m_lookToRing = LookToRing::Full;
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
    initGrid(doc, 16, 16, 1080, 900, 6, 12, theme);
    PageGrid& grid = doc.grids[0];
    adoptCallerBoard(grid, m_pages.pageBehind(QLatin1String(kLiveLookToMap)));
    grid.rows = 16;
    grid.columns = 16;
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
    grid.cells.push_back(title);
    grid.cells.push_back(cell(QStringLiteral("done"), QStringLiteral("Done"), 0, 14,
                              QStringLiteral("lookTo.map.done"), sw.save, 2, {}, {},
                              QStringLiteral("check")));

    PageGrid body = makeNested(QStringLiteral("body"), 1, 0, 1, 2, 16);
    body.colSpan = 16;
    body.rowSpan = 15;
    body.columnTracks = starTracks({1.0, 1.0});

    PageGrid left = makeNested(QStringLiteral("left"), 0, 0, 8, 1, 6);
    left.cells.push_back(sectionLabel(QStringLiteral("h_map"), QStringLiteral("Map"), 0, 0, 1));
    PageGrid mapToggles = makeNested(QStringLiteral("map_tog"), 1, 0, 1, 4, 6);
    mapToggles.cells.push_back(cell(QStringLiteral("en_l"), QStringLiteral("Enable"), 0, 0, {},
                                    QColor(), 1, QStringLiteral("label"),
                                    QStringLiteral("Place origin, then look to drive this map.")));
    PageCell en = cell(QStringLiteral("en_t"), QStringLiteral("On"), 0, 1, enableCmd, QColor(), 1,
                       QStringLiteral("toggle"));
    en.activeState = enableCmd;
    mapToggles.cells.push_back(en);
    mapToggles.cells.push_back(cell(QStringLiteral("prev_l"), QStringLiteral("Preview"), 0, 2, {},
                                    QColor(), 1, QStringLiteral("label"),
                                    QStringLiteral("Draw analog rings at the origin.")));
    PageCell prev =
        cell(QStringLiteral("prev_t"),
             m_lookToPreview ? QStringLiteral("On") : QStringLiteral("Off"), 0, 3,
             QStringLiteral("lookTo.map.preview.toggle"), QColor(), 1, QStringLiteral("toggle"));
    prev.activeState = QStringLiteral("lookTo.map.preview");
    mapToggles.cells.push_back(prev);
    left.subGrids.push_back(std::move(mapToggles));
    left.subGrids.push_back(labeledToggle(
        QStringLiteral("hub"), 2, QStringLiteral("Hub"),
        QStringLiteral("Center pie: speed, place, direction, close."),
        m->hubEnabled ? QStringLiteral("On") : QStringLiteral("Off"),
        QStringLiteral("lookTo.map.hub.toggle"), QStringLiteral("lookTo.map.hub")));
    left.subGrids.push_back(labeled(
        QStringLiteral("hubd"), 3, QStringLiteral("Hub dwell"),
        QStringLiteral("Dwell the hub to open the pie."),
        stepper(QStringLiteral("hubd_s"), QStringLiteral("%1 ms").arg(m->centerDwellMs),
                QStringLiteral("ms"), QStringLiteral("lookTo.map.nudge.centerDwell.dec"),
                QStringLiteral("lookTo.map.nudge.centerDwell.inc"),
                QStringLiteral("lookTo.map.edit.centerDwell"), sw)));
    left.cells.push_back(sectionLabel(QStringLiteral("h_out"), QStringLiteral("Output"), 4, 0, 1));
    left.subGrids.push_back(labeled(
        QStringLiteral("spd"), 5, QStringLiteral("Speed"), QStringLiteral("Peak at 100%."),
        stepper(QStringLiteral("spd_s"), lookToSpeedText(m_lookToDest, m->maxSpeed), {},
                QStringLiteral("lookTo.map.nudge.speed.dec"),
                QStringLiteral("lookTo.map.nudge.speed.inc"),
                QStringLiteral("lookTo.map.edit.speed"), sw)));
    const char* modeIds[] = {"vertical", "horizontal", "both"};
    const char* modeLabs[] = {"V", "H", "Both"};
    left.subGrids.push_back(labeled(
        QStringLiteral("dir"), 6, QStringLiteral("Direction"),
        QStringLiteral("Lock to one axis, or both."),
        choiceRow(QStringLiteral("mode"), modeIds, modeLabs, 3, QStringLiteral("lookTo.map.mode."),
                  QStringLiteral("lookTo.map.mode."))));
    left.subGrids.push_back(labeled(
        QStringLiteral("accel"), 7, QStringLiteral("Accel / s"),
        QStringLiteral("Grows while that axis is engaged."),
        stepper(QStringLiteral("accel_s"), QStringLiteral("%1 /s").arg(m->accelPerSec, 0, 'f', 1),
                QStringLiteral("/s"), QStringLiteral("lookTo.map.nudge.accel.dec"),
                QStringLiteral("lookTo.map.nudge.accel.inc"),
                QStringLiteral("lookTo.map.edit.accel"), sw)));
    body.subGrids.push_back(std::move(left));

    PageGrid right = makeNested(QStringLiteral("right"), 0, 1, 9, 1, 6);
    right.cells.push_back(sectionLabel(QStringLiteral("h_rings"), QStringLiteral("Rings"), 0, 0, 1));
    right.subGrids.push_back(labeled(
        QStringLiteral("dz"), 1, QStringLiteral("Deadzone"),
        QStringLiteral("Inner circle. Output is 0."),
        stepper(QStringLiteral("dz_s"), QStringLiteral("%1 px").arg(m->deadzonePx),
                QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.deadzone.dec"),
                QStringLiteral("lookTo.map.nudge.deadzone.inc"),
                QStringLiteral("lookTo.map.edit.deadzone"), sw)));
    right.subGrids.push_back(labeled(
        QStringLiteral("ramp"), 2, QStringLiteral("Ramp 0–100%"),
        QStringLiteral("Outer edge of the speed ramp."),
        stepper(QStringLiteral("ramp_s"), QStringLiteral("%1 px").arg(m->rampEndPx),
                QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.ramp.dec"),
                QStringLiteral("lookTo.map.nudge.ramp.inc"),
                QStringLiteral("lookTo.map.edit.ramp"), sw)));
    right.subGrids.push_back(labeled(
        QStringLiteral("full"), 3, QStringLiteral("100% outer"),
        QStringLiteral("Outer edge of the 100% ring."),
        stepper(QStringLiteral("full_s"), QStringLiteral("%1 px").arg(m->fullOuterPx),
                QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.full.dec"),
                QStringLiteral("lookTo.map.nudge.full.inc"),
                QStringLiteral("lookTo.map.edit.full"), sw)));
    right.subGrids.push_back(labeledToggle(
        QStringLiteral("outer"), 4, QStringLiteral("Outer deadzone"),
        QStringLiteral("Past 100%, output drops to 0."),
        m->outerDeadzoneEnabled ? QStringLiteral("On") : QStringLiteral("Off"),
        QStringLiteral("lookTo.map.outerDeadzone.toggle"),
        QStringLiteral("lookTo.map.outerDeadzone")));
    right.subGrids.push_back(labeled(
        QStringLiteral("outerr"), 5, QStringLiteral("Outer radius"),
        QStringLiteral("Outer deadzone radius."),
        stepper(QStringLiteral("outer_s"), QStringLiteral("%1 px").arg(m->outerDeadzonePx),
                QStringLiteral("px"), QStringLiteral("lookTo.map.nudge.outer.dec"),
                QStringLiteral("lookTo.map.nudge.outer.inc"),
                QStringLiteral("lookTo.map.edit.outer"), sw)));
    right.cells.push_back(sectionLabel(QStringLiteral("h_style"), QStringLiteral("Style"), 6, 0, 1));
    PageGrid show = makeNested(QStringLiteral("show"), 7, 0, 1, 5, 6);
    show.cells.push_back(cell(QStringLiteral("show_l"), QStringLiteral("Show"), 0, 0, {}, QColor(), 1,
                              QStringLiteral("label"),
                              QStringLiteral("Which overlay parts to draw.")));
    auto showToggle = [&](const QString& id, const QString& lab, const char* cmd, bool on) {
        PageCell t = cell(id, lab, 0, show.cells.size(), QLatin1String(cmd), QColor(), 1,
                          QStringLiteral("toggle"));
        t.activeState = QString::fromLatin1(cmd).remove(QLatin1String(".toggle"));
        t.caption = on ? QStringLiteral("On") : QStringLiteral("Off");
        show.cells.push_back(t);
    };
    showToggle(QStringLiteral("show_pause"), QStringLiteral("Pause"),
               "lookTo.map.show.pause.toggle", m->showPause);
    showToggle(QStringLiteral("show_inner"), QStringLiteral("Inner"),
               "lookTo.map.show.inner.toggle", m->showInnerDeadzone);
    showToggle(QStringLiteral("show_max"), QStringLiteral("Max"), "lookTo.map.show.max.toggle",
               m->showMax);
    showToggle(QStringLiteral("show_outer"), QStringLiteral("Outer"),
               "lookTo.map.show.outer.toggle", m->showOuterDeadzone);
    right.subGrids.push_back(std::move(show));
    PageGrid draw = makeNested(QStringLiteral("draw"), 8, 0, 1, 3, 6);
    draw.cells.push_back(cell(QStringLiteral("draw_l"), QStringLiteral("Draw"), 0, 0, {}, QColor(), 1,
                              QStringLiteral("label"),
                              QStringLiteral("Ring outline and fill.")));
    PageCell border = cell(QStringLiteral("show_border"), QStringLiteral("Border"), 0, 1,
                           QStringLiteral("lookTo.map.show.border.toggle"), QColor(), 1,
                           QStringLiteral("toggle"));
    border.activeState = QStringLiteral("lookTo.map.show.border");
    draw.cells.push_back(border);
    PageCell fill = cell(QStringLiteral("show_fill"), QStringLiteral("Fill"), 0, 2,
                         QStringLiteral("lookTo.map.show.fill.toggle"), QColor(), 1,
                         QStringLiteral("toggle"));
    fill.activeState = QStringLiteral("lookTo.map.show.fill");
    draw.cells.push_back(fill);
    right.subGrids.push_back(std::move(draw));
    body.subGrids.push_back(std::move(right));
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
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.ramp.dec"),
                               nudgeRing(LookToRing::Ramp, -1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.ramp.inc"),
                               nudgeRing(LookToRing::Ramp, +1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.full.dec"),
                               nudgeRing(LookToRing::Full, -1));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.nudge.full.inc"),
                               nudgeRing(LookToRing::Full, +1));
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
    auto toggleShow = [this](bool LookToMapSettings::* field) {
        return [this, field](QString*) {
            if (LookToMapSettings* c = lookToDraft()) {
                c->*field = !(c->*field);
                commitLookToDraft(*c);
                refreshLookToEditor();
            }
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.pause.toggle"),
                               toggleShow(&LookToMapSettings::showPause));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.inner.toggle"),
                               toggleShow(&LookToMapSettings::showInnerDeadzone));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.max.toggle"),
                               toggleShow(&LookToMapSettings::showMax));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.outer.toggle"),
                               toggleShow(&LookToMapSettings::showOuterDeadzone));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.border.toggle"),
                               toggleShow(&LookToMapSettings::showBorder));
    m_commands.registerBuiltin(QStringLiteral("lookTo.map.show.fill.toggle"),
                               toggleShow(&LookToMapSettings::showFill));
}

} // namespace gazer
