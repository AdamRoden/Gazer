#include "layout/PageCompose.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QtTest>
#include <optional>

using namespace gazer;

namespace {

const PageTarget* targetById(const QVector<PageTarget>& targets, const QString& id)
{
    for (const PageTarget& t : targets) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

QString layoutPath(const QString& id)
{
    return QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/") + id
           + QStringLiteral(".xml");
}

bool loadLayout(const QString& id, PageDocument& doc, QString* err)
{
    return PageLoader::loadFromFile(layoutPath(id), doc, err);
}

} // namespace

class SettingsLayoutTest final : public QObject {
    Q_OBJECT

private slots:
    void pagesAnchorTop();
    void tabsEqualWidth();
    void timingSectionUsesRowWeights();
    void stepperWidths();
    void valueLabelKeepsKey();
    void ltsHasNoMaxSpeedOrPlaceCursor();
    void overlayIdsAreUnique();
    void hubOpensBasicBoards();
    void adminCaptureModes();
    void presetsComeFirst();
    void zoomPlaceAndGazeAreBoards();
    void choiceAndToggleRoles();
    void themeHasFlashModes();
    void themeHasHoverRow();
    void dwellKeepsGraceOnPage();
    void zoomAndSetupHoldSession();
    void gazeOmitsSplash();
    void hostInlinesDwellBody();
};

void SettingsLayoutTest::pagesAnchorTop()
{
    const QStringList ids = {QStringLiteral("main_settings_dwell"),
                             QStringLiteral("main_settings_zoom"),
                             QStringLiteral("main_settings_place"),
                             QStringLiteral("main_settings_gaze"),
                             QStringLiteral("main_settings_speak"),
                             QStringLiteral("main_settings_theme"),
                             QStringLiteral("main_settings_head_pose"),
                             QStringLiteral("main_settings_setup")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        QVERIFY2(loadLayout(id, doc, &err), qPrintable(err));
        QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
        QCOMPARE(doc.grids[0].desktopMode, true);
        QCOMPARE(PageDimParse::token(doc.grids[0].size.x),
                 QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
        QCOMPARE(PageDimParse::token(doc.grids[0].size.y), QStringLiteral("A_ScreenHeight"));
        QVERIFY(!doc.grids[0].style.background.isSet());
        QVERIFY(doc.styles.contains(QStringLiteral("plain")));
        QVERIFY(doc.styles.contains(QStringLiteral("join")));
        QVERIFY(doc.styles.contains(QStringLiteral("group")));
        QVERIFY(!doc.findGrid(QStringLiteral("tabs")));
        const PageCell* title = doc.findCell(QStringLiteral("page_title"));
        QVERIFY(title);
        QCOMPARE(title->row, 0);
        QCOMPARE(title->role, QStringLiteral("label"));
        QCOMPARE(title->textStyle, QStringLiteral("title"));
        QVERIFY(!doc.grids[0].rowTracks.isEmpty());
        QCOMPARE(PageDimParse::token(doc.grids[0].rowTracks[0]), QStringLiteral("96"));
        QCOMPARE(doc.styles.value(QStringLiteral("group")).resolvedRadius().first(), 16.0);
        QCOMPARE(doc.styles.value(QStringLiteral("row")).resolvedThickness().first(), 0.0);
    }
}

void SettingsLayoutTest::tabsEqualWidth()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_host"), doc, &err), qPrintable(err));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(doc.findGrid(QStringLiteral("body")));
    QCOMPARE(doc.findGrid(QStringLiteral("body"))->src, QStringLiteral("main_settings_dwell"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 9);
    QCOMPARE(tabs->cells.size(), 9);
    int tabRoles = 0;
    bool hasDone = false;
    for (const PageCell& c : tabs->cells) {
        if (c.role == QLatin1String("tab")) {
            ++tabRoles;
        }
        if (c.id == QLatin1String("tab_done")) {
            hasDone = true;
            QVERIFY(c.isInteractive());
            QCOMPARE(c.col, 8);
        }
    }
    QCOMPARE(tabRoles, 8);
    QVERIFY(hasDone);
    QCOMPARE(doc.findCell(QStringLiteral("tab_dwell"))->actions[0].targetId,
             QStringLiteral("main_settings_dwell"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_zoom"))->actions[0].targetId,
             QStringLiteral("main_settings_zoom"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_place"))->actions[0].targetId,
             QStringLiteral("main_settings_place"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_gaze"))->actions[0].targetId,
             QStringLiteral("main_settings_gaze"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_head"))->actions[0].targetId,
             QStringLiteral("main_settings_head_pose"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_speak"))->actions[0].targetId,
             QStringLiteral("main_settings_speak"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_theme"))->actions[0].targetId,
             QStringLiteral("main_settings_theme"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_setup"))->actions[0].targetId,
             QStringLiteral("main_settings_setup"));
    QVERIFY(!doc.findCell(QStringLiteral("tab_pointer")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_look")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_advanced")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_speed")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_assist")));
}

void SettingsLayoutTest::timingSectionUsesRowWeights()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    const PageGrid* dwell = doc.findGrid(QStringLiteral("sec_dwell"));
    QVERIFY(dwell);
    QCOMPARE(dwell->styleId, QStringLiteral("group"));
    QCOMPARE(dwell->rows, 4);
    QCOMPARE(PageDimParse::tokenList(dwell->rowTracks), QStringLiteral("*,2*,2*,2*"));
    const PageGrid* standard = doc.findGrid(QStringLiteral("row_dwell"));
    QVERIFY(standard);
    QCOMPARE(standard->styleId, QStringLiteral("row"));
    QCOMPARE(standard->rowSpan, 1);
    QCOMPARE(standard->row, 1);
    const PageGrid* boost = doc.findGrid(QStringLiteral("row_rapid_dwell"));
    QVERIFY(boost);
    QCOMPARE(boost->row, 2);

    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* header = targetById(t, QStringLiteral("h_dwell"));
    const PageTarget* desc = targetById(t, QStringLiteral("rapid_dwell_label"));
    QVERIFY(header);
    QVERIFY(desc);
    QVERIFY(qAbs(2.0 * header->geom.visual.height() - desc->geom.visual.height()) < 1.5);
}

void SettingsLayoutTest::stepperWidths()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    const PageGrid* act = doc.findGrid(QStringLiteral("act_scan"));
    QVERIFY(act);
    QCOMPARE(act->gapPx, 0);
    QCOMPARE(act->columns, 9);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* dec = targetById(t, QStringLiteral("scan_dec"));
    const PageTarget* val = targetById(t, QStringLiteral("scan_val"));
    const PageTarget* inc = targetById(t, QStringLiteral("scan_inc"));
    const PageTarget* edit = targetById(t, QStringLiteral("scan_edit"));
    QVERIFY(dec && val && inc && edit);
    QCOMPARE(val->role, QStringLiteral("value"));
    QCOMPARE(dec->geom.visual.width(), inc->geom.visual.width());
    QCOMPARE(dec->geom.visual.width(), edit->geom.visual.width());
    QVERIFY(val->geom.visual.width() > dec->geom.visual.width());
    QVERIFY(qAbs(dec->geom.visual.right() - val->geom.visual.left()) < 0.75);
}

void SettingsLayoutTest::valueLabelKeepsKey()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* standard = targetById(t, QStringLiteral("dwell_value"));
    QVERIFY(standard);
    QCOMPARE(standard->role, QStringLiteral("value"));
    QCOMPARE(standard->settingKey, QStringLiteral("dwellMs"));
    const PageTarget* rapid = targetById(t, QStringLiteral("rapid_dwell_value"));
    QVERIFY(rapid);
    QCOMPARE(rapid->settingKey, QStringLiteral("rapidDwellMs"));
    const PageTarget* scan = targetById(t, QStringLiteral("scan_val"));
    QVERIFY(scan);
    QCOMPARE(scan->settingKey, QStringLiteral("scanGraceMs"));
    QVERIFY(!targetById(t, QStringLiteral("dd_scan_val")));
    QVERIFY(!targetById(t, QStringLiteral("pd_val")));
}

void SettingsLayoutTest::ltsHasNoMaxSpeedOrPlaceCursor()
{
    QFile f(layoutPath(QStringLiteral("main_settings_gaze")));
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QByteArray xml = f.readAll();
    QVERIFY(!xml.contains("ltsMaxNotchesPerSec"));
    QVERIFY(!xml.contains("lts.placeCursor"));
    QVERIFY(!xml.contains("Place cursor first"));
    QVERIFY(!xml.contains("ltsPlaceCursorFirst"));
}

void SettingsLayoutTest::overlayIdsAreUnique()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), doc, &err), qPrintable(err));
    const QStringList gazeIds = PageEdit::allIds(doc);
    QSet<QString> seen;
    for (const QString& id : gazeIds) {
        QVERIFY2(!seen.contains(id), qPrintable(id));
        seen.insert(id);
    }
    QVERIFY(seen.contains(QStringLiteral("row_mouse")));
    QVERIFY(seen.contains(QStringLiteral("lts_on")));
    QVERIFY(seen.contains(QStringLiteral("mouse_on")));
    QVERIFY(seen.contains(QStringLiteral("magnifier")));
    QVERIFY(!seen.contains(QStringLiteral("combo_on")));
    QVERIFY(!seen.contains(QStringLiteral("splash_on")));

    PageDocument place;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_place"), place, &err), qPrintable(err));
    const QStringList placeIds = PageEdit::allIds(place);
    QSet<QString> placeSeen;
    for (const QString& id : placeIds) {
        QVERIFY2(!placeSeen.contains(id), qPrintable(id));
        placeSeen.insert(id);
    }
    QVERIFY(placeSeen.contains(QStringLiteral("row_combo_style")));
    QVERIFY(placeSeen.contains(QStringLiteral("combo_on")));
    QVERIFY(placeSeen.contains(QStringLiteral("pd_val")));
}

void SettingsLayoutTest::hubOpensBasicBoards()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings"), doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("main_settings"));
    QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
    QCOMPARE(PageDimParse::token(doc.grids[0].size.x),
             QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
    QCOMPARE(PageDimParse::token(doc.grids[0].size.y), QStringLiteral("A_ScreenHeight"));
    const QStringList pages = {QStringLiteral("main_settings_dwell"),
                               QStringLiteral("main_settings_zoom"),
                               QStringLiteral("main_settings_place"),
                               QStringLiteral("main_settings_gaze"),
                               QStringLiteral("main_settings_head_pose"),
                               QStringLiteral("main_settings_speak"),
                               QStringLiteral("main_settings_theme"),
                               QStringLiteral("main_settings_setup")};
    QSet<QString> opened;
    QCOMPARE(doc.grids[0].columns, 4);
    QVERIFY(!doc.findCell(QStringLiteral("done")));
    QVERIFY(!doc.findCell(QStringLiteral("open_advanced")));
    QVERIFY(!doc.findCell(QStringLiteral("open_pointer")));
    QVERIFY(!doc.findCell(QStringLiteral("open_look")));
    for (const PageGrid& g : doc.grids) {
        for (const PageCell& c : g.cells) {
            for (const PageAction& a : c.actions) {
                if (a.type == PageActionType::HostPage) {
                    opened.insert(a.targetId);
                    QCOMPARE(a.hostId, QStringLiteral("main_settings_host"));
                }
            }
        }
    }
    for (const QString& id : pages) {
        QVERIFY2(opened.contains(id), qPrintable(id));
    }
    QCOMPARE(opened.size(), 8);
    QVERIFY(!opened.contains(QStringLiteral("main_settings_pointer")));
    QVERIFY(!opened.contains(QStringLiteral("main_settings_look")));
    QVERIFY(!opened.contains(QStringLiteral("main_settings_indicators")));
    QVERIFY(!opened.contains(QStringLiteral("main_settings_tools")));
    PageDocument speak;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speak"), speak, &err), qPrintable(err));
    QCOMPARE(speak.findCell(QStringLiteral("speed_value"))->settingKey,
             QStringLiteral("speechSpeed"));
    QCOMPARE(speak.findCell(QStringLiteral("volume_value"))->settingKey,
             QStringLiteral("speechVolume"));
    PageDocument host;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_host"), host, &err), qPrintable(err));
    QVERIFY(host.findCell(QStringLiteral("tab_speak")));
    QVERIFY(!host.findCell(QStringLiteral("tab_advanced")));
    PageDocument head;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_head_pose"), head, &err), qPrintable(err));
    QVERIFY(head.findCell(QStringLiteral("head_preview")));
    QCOMPARE(head.findCell(QStringLiteral("head_preview"))->role, QStringLiteral("headpreview"));
    QVERIFY(head.findCell(QStringLiteral("pose_curve")));
    QCOMPARE(head.findCell(QStringLiteral("pose_curve"))->role, QStringLiteral("curvefield"));
    QVERIFY(!head.findCell(QStringLiteral("pose_curve"))->isInteractive());
    QCOMPARE(head.findCell(QStringLiteral("pose_curve"))->row, 2);
    QVERIFY(head.findCell(QStringLiteral("hp_enable")));
    QVERIFY(head.findCell(QStringLiteral("hp_recenter")));
    QVERIFY(!head.findCell(QStringLiteral("hp_add")));
    QVERIFY(!head.findCell(QStringLiteral("axis_yaw")));
    QVERIFY(!head.findCell(QStringLiteral("axis_pitch")));
    QVERIFY(!head.findCell(QStringLiteral("axis_roll")));
    QVERIFY(!head.findCell(QStringLiteral("axis_x")));
    QVERIFY(!head.findCell(QStringLiteral("axis_y")));
    QVERIFY(!head.findCell(QStringLiteral("axis_z")));
    const PageGrid* preview = head.findGrid(QStringLiteral("sec_preview"));
    const PageGrid* maps = head.findGrid(QStringLiteral("sec_maps"));
    QVERIFY(preview);
    QVERIFY(maps);
    QCOMPARE(preview->rows, 3);
    QVERIFY(head.findGrid(QStringLiteral("row_master")));
}

void SettingsLayoutTest::presetsComeFirst()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    const PageGrid* presets = doc.findGrid(QStringLiteral("sec_presets"));
    const PageGrid* dwell = doc.findGrid(QStringLiteral("sec_dwell"));
    QVERIFY(presets);
    QVERIFY(dwell);
    QVERIFY(presets->row < dwell->row);
    QCOMPARE(doc.findGrid(QStringLiteral("row_dwell"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_rapid_dwell"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("row_key_gravity"))->row, 3);
    QCOMPARE(doc.findCell(QStringLiteral("dwell_label"))->label, QStringLiteral("Standard"));
    QCOMPARE(doc.findCell(QStringLiteral("rapid_dwell_label"))->label, QStringLiteral("Rapid"));
    QCOMPARE(doc.findCell(QStringLiteral("gravity_label"))->label, QStringLiteral("Likely keys"));
    QCOMPARE(doc.findCell(QStringLiteral("gravity_val"))->settingKey, QStringLiteral("keyGravity"));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_rapid")));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_designer")));
    const PageCell* slow = doc.findCell(QStringLiteral("p_slow"));
    QVERIFY(slow);
    QCOMPARE(slow->role, QStringLiteral("choice"));
    QCOMPARE(doc.findCell(QStringLiteral("p_custom"))->role, QStringLiteral("choice"));
    QVERIFY(doc.findCell(QStringLiteral("p_save"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("p_restore"))->isInteractive());
}

void SettingsLayoutTest::zoomPlaceAndGazeAreBoards()
{
    PageDocument zoom;
    PageDocument place;
    PageDocument gaze;
    PageDocument dwell;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_zoom"), zoom, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_place"), place, &err),
             qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), gaze, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), dwell, &err), qPrintable(err));
    QCOMPARE(zoom.findCell(QStringLiteral("page_title"))->label, QStringLiteral("Zoom"));
    QCOMPARE(place.findCell(QStringLiteral("page_title"))->label, QStringLiteral("Place"));
    QCOMPARE(gaze.findCell(QStringLiteral("page_title"))->label, QStringLiteral("Gaze"));
    QCOMPARE(zoom.findCell(QStringLiteral("zl_val"))->settingKey, QStringLiteral("pickZoom"));
    QCOMPARE(zoom.findCell(QStringLiteral("zs_val"))->settingKey, QStringLiteral("pickWindowPx"));
    QCOMPARE(zoom.findCell(QStringLiteral("zd_val"))->settingKey, QStringLiteral("magPickDwellMs"));
    QCOMPARE(zoom.findCell(QStringLiteral("fsd_val"))->settingKey,
             QStringLiteral("mouseMoveForesightDwellMs"));
    QCOMPARE(zoom.findCell(QStringLiteral("fsh_val"))->settingKey,
             QStringLiteral("mouseMoveForesightHoldMs"));
    QVERIFY(zoom.findCell(QStringLiteral("zi_cur")));
    QVERIFY(zoom.findCell(QStringLiteral("zi_dot")));
    QVERIFY(zoom.findCell(QStringLiteral("zi_xh")));
    QVERIFY(zoom.findCell(QStringLiteral("zi_gz")));
    QVERIFY(zoom.findCell(QStringLiteral("test_move")));
    QVERIFY(!zoom.findCell(QStringLiteral("zm_none")));
    QCOMPARE(zoom.findCell(QStringLiteral("zm_pre"))->role, QStringLiteral("toggle"));
    QCOMPARE(zoom.findCell(QStringLiteral("zm_fs"))->role, QStringLiteral("toggle"));
    QCOMPARE(zoom.findCell(QStringLiteral("zm_fs2"))->role, QStringLiteral("toggle"));
    QVERIFY(!dwell.findCell(QStringLiteral("zd_val")));
    QVERIFY(!gaze.findCell(QStringLiteral("zl_val")));
    QVERIFY(!gaze.findCell(QStringLiteral("test_move")));
    QCOMPARE(place.findCell(QStringLiteral("pd_val"))->settingKey,
             QStringLiteral("mouseMoveDwellMs"));
    QCOMPARE(place.findCell(QStringLiteral("pg_val"))->settingKey,
             QStringLiteral("mouseMoveSelectTimeoutMs"));
    QVERIFY(place.findCell(QStringLiteral("pi_cur")));
    QVERIFY(place.findCell(QStringLiteral("pi_dot")));
    QVERIFY(place.findCell(QStringLiteral("pi_xh")));
    QVERIFY(place.findCell(QStringLiteral("test_move")));
    QCOMPARE(place.findCell(QStringLiteral("test_label"))->label, QStringLiteral("Try"));
    QCOMPARE(place.findGrid(QStringLiteral("row_test"))->row, 4);
    QCOMPARE(place.findCell(QStringLiteral("combo_on_l"))->label, QStringLiteral("Try"));
    QCOMPARE(place.findGrid(QStringLiteral("row_combo_on"))->row, 5);
    QCOMPARE(place.findGrid(QStringLiteral("row_combo_inner"))->row, 1);
    QVERIFY(place.findCell(QStringLiteral("combo_on")));
    QVERIFY(!place.findCell(QStringLiteral("pp_r")));
    QVERIFY(!place.findCell(QStringLiteral("bp_r")));
    QVERIFY(!place.findCell(QStringLiteral("zi_gz")));
}

void SettingsLayoutTest::choiceAndToggleRoles()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    const PageCell* ring = doc.findCell(QStringLiteral("bp_r"));
    QVERIFY(ring);
    QCOMPARE(ring->role, QStringLiteral("toggle"));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_theme"), doc, &err), qPrintable(err));
    const PageCell* title = doc.findCell(QStringLiteral("page_title"));
    QVERIFY(title);
    QCOMPARE(title->label, QStringLiteral("Theme"));
    QCOMPARE(title->role, QStringLiteral("label"));
    const PageCell* light = doc.findCell(QStringLiteral("light"));
    QVERIFY(light);
    QCOMPARE(light->role, QStringLiteral("choice"));
    QVERIFY(doc.findCell(QStringLiteral("dark")));
    QVERIFY(!doc.findCell(QStringLiteral("light_tint")));
    QVERIFY(!doc.findCell(QStringLiteral("dark_tint")));
    QVERIFY(!doc.findCell(QStringLiteral("custom")));
    QVERIFY(!doc.findCell(QStringLiteral("scheme_blue")));
    const PageGrid* columns = doc.findGrid(QStringLiteral("theme_columns"));
    const PageGrid* surfaces = doc.findGrid(QStringLiteral("sec_surfaces"));
    const PageGrid* accents = doc.findGrid(QStringLiteral("sec_accents"));
    QVERIFY(columns);
    QVERIFY(surfaces);
    QVERIFY(accents);
    QCOMPARE(columns->columns, 2);
    QCOMPARE(surfaces->col, 0);
    QCOMPARE(accents->col, 1);
    QCOMPARE(doc.findCell(QStringLiteral("h_surfaces"))->label, QStringLiteral("Background"));
    QCOMPARE(doc.findCell(QStringLiteral("h_accents"))->label, QStringLiteral("Accent Colors"));
    QCOMPARE(doc.findCell(QStringLiteral("h_suggestions"))->label,
             QStringLiteral("Recommended Progress Choices"));
    QCOMPARE(doc.findCell(QStringLiteral("pc"))->label, QStringLiteral("Highlight"));
    QCOMPARE(doc.findCell(QStringLiteral("sc"))->label, QStringLiteral("Progress"));
    for (int i = 0; i < 5; ++i) {
        const PageCell* shade = doc.findCell(QStringLiteral("shade_bg_%1").arg(i));
        QVERIFY2(shade, qPrintable(QStringLiteral("shade_bg_%1").arg(i)));
        QCOMPARE(shade->role, QStringLiteral("choice"));
        QVERIFY(shade->isInteractive());
        QCOMPARE(shade->actions[0].command, QStringLiteral("theme.brightness.%1").arg(i));
    }
    QVERIFY(doc.findCell(QStringLiteral("tint_none")));
    QCOMPARE(doc.findCell(QStringLiteral("tint_none"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_primary"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_complementary"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_analogous1"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_analogous2"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_tertiary1"))->role, QStringLiteral("swatch"));
    QCOMPARE(doc.findCell(QStringLiteral("tint_tertiary2"))->role, QStringLiteral("swatch"));
    const PageCell* pSwatch = doc.findCell(QStringLiteral("p_swatch"));
    const PageCell* sSwatch = doc.findCell(QStringLiteral("s_swatch"));
    QVERIFY(pSwatch);
    QVERIFY(sSwatch);
    QCOMPARE(pSwatch->role, QStringLiteral("swatch"));
    QCOMPARE(sSwatch->role, QStringLiteral("swatch"));
    QCOMPARE(pSwatch->actions[0].command, QStringLiteral("settings.edit.color.customPrimaryColor"));
    QCOMPARE(sSwatch->actions[0].command, QStringLiteral("settings.edit.color.customSecondaryColor"));
    QVERIFY(!doc.findGrid(QStringLiteral("row_hue")));
    QVERIFY(!doc.findCell(QStringLiteral("track_h")));
    QVERIFY(!doc.findCell(QStringLiteral("hex")));
    QVERIFY(doc.findGrid(QStringLiteral("row_pal_analogous1")));
    QVERIFY(doc.findGrid(QStringLiteral("row_pal_analogous2")));
    QVERIFY(doc.findGrid(QStringLiteral("row_pal_tertiary1")));
    QVERIFY(doc.findGrid(QStringLiteral("row_pal_tertiary2")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pal_analogous")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pal_tertiary")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pal_triadic1")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pal_triadic2")));
    const PageGrid* palPrimary = doc.findGrid(QStringLiteral("row_pal_primary"));
    QVERIFY(palPrimary);
    QCOMPARE(palPrimary->columns, 9);
    QCOMPARE(palPrimary->gapPx, 0);
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_primary"))->label, QStringLiteral("Primary"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_complementary"))->label,
             QStringLiteral("Complementary"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_analogous1"))->label, QStringLiteral("Analogous 1"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_analogous2"))->label, QStringLiteral("Analogous 2"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_tertiary1"))->label, QStringLiteral("Tertiary 1"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_tertiary2"))->label, QStringLiteral("Tertiary 2"));
    QCOMPARE(doc.findGrid(QStringLiteral("row_pal_analogous1"))->row + 1,
             doc.findGrid(QStringLiteral("row_pal_analogous2"))->row);
    QCOMPARE(doc.findGrid(QStringLiteral("row_pal_analogous2"))->row + 1,
             doc.findGrid(QStringLiteral("row_pal_tertiary1"))->row);
    for (int i = 1; i <= 9; ++i) {
        const PageCell* shade = doc.findCell(QStringLiteral("pal_primary_%1").arg(i));
        QVERIFY2(shade, qPrintable(QStringLiteral("pal_primary_%1").arg(i)));
        QVERIFY(shade->isInteractive());
        QVERIFY(shade->label.isEmpty());
        QCOMPARE(shade->actions[0].command,
                 QStringLiteral("settings.theme.shade.primary.%1").arg(i));
        QVERIFY(doc.findCell(QStringLiteral("pal_analogous1_%1").arg(i)));
        QVERIFY(doc.findCell(QStringLiteral("pal_analogous2_%1").arg(i)));
        QVERIFY(doc.findCell(QStringLiteral("pal_tertiary1_%1").arg(i)));
        QVERIFY(doc.findCell(QStringLiteral("pal_tertiary2_%1").arg(i)));
        QVERIFY(doc.findCell(QStringLiteral("pal_complementary_%1").arg(i)));
    }
    QVERIFY(!doc.findCell(QStringLiteral("pal_primary_0")));
    QVERIFY(!doc.findCell(QStringLiteral("pal_analogous1_0")));
}

void SettingsLayoutTest::themeHasFlashModes()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_theme"), doc, &err), qPrintable(err));
    const PageGrid* surfaces = doc.findGrid(QStringLiteral("sec_surfaces"));
    QVERIFY(surfaces);
    QCOMPARE(surfaces->rows, 6);
    const PageGrid* actFlash = doc.findGrid(QStringLiteral("act_flash"));
    QVERIFY(actFlash);
    QCOMPARE(actFlash->row, 5);
    QCOMPARE(actFlash->columns, 3);
    QVERIFY(!doc.findCell(QStringLiteral("fl_fg")));
    const PageCell* flName = doc.findCell(QStringLiteral("fl_name"));
    const PageCell* flCustom = doc.findCell(QStringLiteral("fl_custom"));
    const PageCell* flSw = doc.findCell(QStringLiteral("fl_sw"));
    const PageCell* flVal = doc.findCell(QStringLiteral("fl_val"));
    QVERIFY(flName);
    QVERIFY(flCustom);
    QVERIFY(flSw);
    QVERIFY(flVal);
    QCOMPARE(flName->caption, QStringLiteral("Defaults to foreground"));
    QCOMPARE(flVal->settingKey, QStringLiteral("flashMs"));
    QCOMPARE(flCustom->role, QStringLiteral("toggle"));
    QCOMPARE(flCustom->actions[0].command, QStringLiteral("settings.flash.custom.toggle"));
    QCOMPARE(flSw->role, QStringLiteral("swatch"));
    QCOMPARE(flSw->settingKey, QStringLiteral("flashColor"));
    QCOMPARE(flSw->visibleWhen, QStringLiteral("flash_custom"));
    QCOMPARE(flSw->actions[0].command, QStringLiteral("settings.edit.color.flashColor"));
}

void SettingsLayoutTest::themeHasHoverRow()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_theme"), doc, &err), qPrintable(err));
    const PageGrid* actHover = doc.findGrid(QStringLiteral("act_hover"));
    QVERIFY(actHover);
    QCOMPARE(actHover->row, 4);
    QCOMPARE(actHover->columns, 3);
    const PageCell* hvName = doc.findCell(QStringLiteral("hv_name"));
    const PageCell* hvSw = doc.findCell(QStringLiteral("hv_sw"));
    const PageCell* hvVal = doc.findCell(QStringLiteral("hv_val"));
    const PageCell* hvCustom = doc.findCell(QStringLiteral("hv_custom"));
    QVERIFY(hvName);
    QVERIFY(hvSw);
    QVERIFY(hvVal);
    QVERIFY(hvCustom);
    QVERIFY(!doc.findCell(QStringLiteral("hv_prog")));
    QCOMPARE(hvName->label, QStringLiteral("Hover"));
    QCOMPARE(hvName->caption, QStringLiteral("Defaults to progress color"));
    QCOMPARE(hvVal->settingKey, QStringLiteral("hoverBorderWeight"));
    QCOMPARE(hvVal->role, QStringLiteral("value"));
    QCOMPARE(hvCustom->role, QStringLiteral("toggle"));
    QCOMPARE(hvCustom->actions[0].command, QStringLiteral("settings.hover.custom.toggle"));
    QCOMPARE(hvSw->role, QStringLiteral("swatch"));
    QCOMPARE(hvSw->settingKey, QStringLiteral("hoverColor"));
    QCOMPARE(hvSw->visibleWhen, QStringLiteral("hover_custom"));
    QCOMPARE(hvSw->actions[0].command, QStringLiteral("settings.edit.color.hoverColor"));
    QCOMPARE(doc.findGrid(QStringLiteral("act_hv_w"))->col, 0);
    QCOMPARE(hvCustom->col, 1);
    QCOMPARE(hvSw->col, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("act_flash"))->row, actHover->row + 1);
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    QVERIFY(!doc.findCell(QStringLiteral("bp_b")));
    QVERIFY(!doc.findCell(QStringLiteral("pp_b")));
    QCOMPARE(doc.findGrid(QStringLiteral("act_bp"))->columns, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("act_pp"))->columns, 3);
}

void SettingsLayoutTest::dwellKeepsGraceOnPage()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->rows, 4);
    QVERIFY(!doc.findCell(QStringLiteral("open_advanced")));
    QVERIFY(!doc.findCell(QStringLiteral("pd_val")));
    QVERIFY(!doc.findCell(QStringLiteral("zd_val")));
    QCOMPARE(doc.findCell(QStringLiteral("scan_val"))->settingKey, QStringLiteral("scanGraceMs"));
    QCOMPARE(doc.findCell(QStringLiteral("grace_val"))->settingKey, QStringLiteral("dwellGraceMs"));
    QCOMPARE(doc.findGrid(QStringLiteral("row_scan"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_grace"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_grace"))->rows, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("row_bp"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_pp"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("act_bp"))->columns, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("act_pp"))->columns, 3);
    QCOMPARE(doc.findCell(QStringLiteral("bp_r"))->role, QStringLiteral("toggle"));
    QCOMPARE(doc.findCell(QStringLiteral("pp_r"))->role, QStringLiteral("toggle"));
    QVERIFY(!doc.findCell(QStringLiteral("bp_b")));
    QVERIFY(!doc.findCell(QStringLiteral("pp_b")));
    QVERIFY(!doc.findGrid(QStringLiteral("tabs")));
}

void SettingsLayoutTest::zoomAndSetupHoldSession()
{
    PageDocument zoom;
    PageDocument setup;
    PageDocument gaze;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_zoom"), zoom, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_setup"), setup, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), gaze, &err), qPrintable(err));
    QCOMPARE(zoom.findCell(QStringLiteral("fsh_val"))->settingKey,
             QStringLiteral("mouseMoveForesightHoldMs"));
    QVERIFY(!zoom.findCell(QStringLiteral("aci_val")));
    QVERIFY(!zoom.findCell(QStringLiteral("splash_on")));
    QCOMPARE(setup.findCell(QStringLiteral("aci_val"))->settingKey,
             QStringLiteral("layoutAutoCloseIdleMs"));
    QCOMPARE(setup.findCell(QStringLiteral("ac_on"))->role, QStringLiteral("toggle"));
    QCOMPARE(setup.findGrid(QStringLiteral("board"))->rows, 6);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_session"))->row, 5);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_session"))->rows, 3);
    const PageCell* on = setup.findCell(QStringLiteral("splash_on"));
    const PageCell* play = setup.findCell(QStringLiteral("splash_play"));
    QVERIFY(on);
    QVERIFY(play);
    QCOMPARE(on->role, QStringLiteral("toggle"));
    QCOMPARE(on->activeState, QStringLiteral("settings.session.showSplash.toggle"));
    QCOMPARE(on->actions[0].command, QStringLiteral("settings.session.showSplash.toggle"));
    QCOMPARE(play->actions[0].command, QStringLiteral("settings.session.showSplash.play"));
    QVERIFY(play->isInteractive());
    QVERIFY(!gaze.findCell(QStringLiteral("splash_on")));
    QVERIFY(!gaze.findCell(QStringLiteral("splash_play")));
    QVERIFY(!setup.findCell(QStringLiteral("fl_sw")));
    QVERIFY(!setup.findCell(QStringLiteral("key_edit")));
}

void SettingsLayoutTest::gazeOmitsSplash()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->rows, 2);
    QVERIFY(!doc.findGrid(QStringLiteral("sec_vigem")));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_tr")));
    QVERIFY(!doc.findCell(QStringLiteral("vigem_install")));
    QVERIFY(!doc.findCell(QStringLiteral("splash_on")));
    QVERIFY(!doc.findCell(QStringLiteral("tr_a")));
    QCOMPARE(doc.findGrid(QStringLiteral("row_follow"))->row, 1);
    QVERIFY(doc.findCell(QStringLiteral("f_slow")));
    QVERIFY(doc.findCell(QStringLiteral("reticle")));
    QVERIFY(doc.findCell(QStringLiteral("follow")));
    QVERIFY(doc.findCell(QStringLiteral("magnifier")));
    QCOMPARE(doc.findCell(QStringLiteral("z_val"))->settingKey, QStringLiteral("magZoom"));
    QCOMPARE(doc.findCell(QStringLiteral("l_val"))->settingKey, QStringLiteral("magLensSize"));
    QVERIFY(doc.findCell(QStringLiteral("lts_on")));
    QVERIFY(doc.findCell(QStringLiteral("mouse_on")));
    QVERIFY(doc.findCell(QStringLiteral("lstick_on")));
    QVERIFY(doc.findCell(QStringLiteral("rstick_on")));
    QVERIFY(doc.findCell(QStringLiteral("look_hint")));
    QVERIFY(!doc.findCell(QStringLiteral("combo_on")));
}

void SettingsLayoutTest::adminCaptureModes()
{
    PageDocument setup;
    PageDocument speak;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_setup"), setup, &err), qPrintable(err));
    const PageCell* all = setup.findCell(QStringLiteral("cap_all"));
    const PageCell* pages = setup.findCell(QStringLiteral("cap_pages"));
    const PageCell* none = setup.findCell(QStringLiteral("cap_none"));
    QVERIFY(all && pages && none);
    QCOMPARE(all->role, QStringLiteral("choice"));
    QCOMPARE(pages->role, QStringLiteral("choice"));
    QCOMPARE(none->role, QStringLiteral("choice"));
    QCOMPARE(all->actions[0].command, QStringLiteral("settings.capture.all"));
    QCOMPARE(pages->actions[0].command, QStringLiteral("settings.capture.pages"));
    QCOMPARE(none->actions[0].command, QStringLiteral("settings.capture.none"));
    QCOMPARE(all->activeState, all->actions[0].command);
    QCOMPARE(pages->activeState, pages->actions[0].command);
    QCOMPARE(none->activeState, none->actions[0].command);
    QCOMPARE(setup.findCell(QStringLiteral("tr_a"))->actions[0].command,
             QStringLiteral("settings.tracker.auto"));
    QCOMPARE(setup.findCell(QStringLiteral("vigem_install"))->actions[0].command,
             QStringLiteral("settings.vigem.install"));
    QCOMPARE(setup.findCell(QStringLiteral("vigem_refresh"))->actions[0].command,
             QStringLiteral("settings.vigem.refresh"));
    QCOMPARE(setup.findGrid(QStringLiteral("sec_tr"))->row, 1);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_vigem"))->row, 2);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_capture"))->row, 3);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_splash"))->row, 4);
    QVERIFY(!setup.findCell(QStringLiteral("key_edit")));
    QVERIFY(!setup.findCell(QStringLiteral("head_preview")));
    QVERIFY(!setup.findGrid(QStringLiteral("sec_speech")));
    QVERIFY(!setup.findGrid(QStringLiteral("tabs")));

    QVERIFY2(loadLayout(QStringLiteral("main_settings_speak"), speak, &err), qPrintable(err));
    QCOMPARE(speak.findCell(QStringLiteral("key_edit"))->actions[0].command,
             QStringLiteral("settings.speech.editKey"));
    QCOMPARE(speak.findGrid(QStringLiteral("sec_speech"))->row, 1);
    QCOMPARE(speak.findGrid(QStringLiteral("sec_speech"))->rows, 5);
    QCOMPARE(speak.findGrid(QStringLiteral("row_key"))->row, 1);
    QCOMPARE(speak.findGrid(QStringLiteral("row_eng"))->row, 2);
    QCOMPARE(speak.findGrid(QStringLiteral("row_spd"))->row, 3);
    QCOMPARE(speak.findGrid(QStringLiteral("row_predict"))->row, 4);
    QCOMPARE(speak.findCell(QStringLiteral("predict_on"))->role, QStringLiteral("toggle"));
    QCOMPARE(speak.findCell(QStringLiteral("predict_on"))->actions[0].command,
             QStringLiteral("settings.speech.predictions.toggle"));
    QVERIFY(speak.findCell(QStringLiteral("m_sapi")));
    QVERIFY(speak.findCell(QStringLiteral("m_turbo")));
    QCOMPARE(speak.findCell(QStringLiteral("m_turbo"))->actions[0].command,
             QStringLiteral("settings.speech.model.eleven_v4_turbo"));
    QVERIFY(!speak.findCell(QStringLiteral("cap_all")));
    QVERIFY(!speak.findCell(QStringLiteral("vigem_install")));
    QVERIFY(!speak.findGrid(QStringLiteral("sec_key")));
    QVERIFY(!speak.findGrid(QStringLiteral("tabs")));
}

void SettingsLayoutTest::hostInlinesDwellBody()
{
    PageDocument host;
    PageDocument dwellDoc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_host"), host, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), dwellDoc, &err), qPrintable(err));
    QHash<QString, PageDocument> hosted;
    hosted.insert(dwellDoc.id, dwellDoc);
    const auto ptrs = PageCompose::pointers(hosted);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t =
        PageHit::collect(host, frame, {}, false, &grids, false, std::nullopt, &ptrs);
    const PageTarget* tab = targetById(t, QStringLiteral("tab_dwell"));
    const PageTarget* tabZoom = targetById(t, QStringLiteral("tab_zoom"));
    const PageTarget* dwell = targetById(t, QStringLiteral("dwell_edit"));
    QVERIFY(tab);
    QVERIFY(tabZoom);
    QVERIFY(dwell);
    QCOMPARE(tab->pageId, QStringLiteral("main_settings_host"));
    QVERIFY(!tab->interactive);
    QVERIFY(tab->actions.isEmpty());
    QVERIFY(tabZoom->interactive);
    QCOMPARE(dwell->pageId, QStringLiteral("main_settings_dwell"));
    QVERIFY(dwell->interactive);
    QVERIFY(!targetById(t, QStringLiteral("open_advanced")));
    QVERIFY(!targetById(t, QStringLiteral("seg_zoom")));
    QVERIFY(tab->geom.visual.center().y() < dwell->geom.visual.center().y());
    const PageTarget* hitTab = PageHit::at(t, tabZoom->geom.visual.center(), 1.0, {}, grids);
    QVERIFY(hitTab);
    QCOMPARE(hitTab->id, QStringLiteral("tab_zoom"));
    const PageTarget* hitBody = PageHit::at(t, dwell->geom.visual.center(), 1.0, {}, grids);
    QVERIFY(hitBody);
    QCOMPARE(hitBody->id, QStringLiteral("dwell_edit"));

    PageDocument zoom;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_zoom"), zoom, &err), qPrintable(err));
    PageGrid* body = host.findGrid(QStringLiteral("body"));
    QVERIFY(body);
    body->src = QStringLiteral("main_settings_zoom");
    QHash<QString, PageDocument> zoomed;
    zoomed.insert(zoom.id, zoom);
    const auto zoomPtrs = PageCompose::pointers(zoomed);
    const QVector<PageTarget> zt =
        PageHit::collect(host, frame, {}, false, nullptr, false, std::nullopt, &zoomPtrs);
    const PageTarget* onZoom = targetById(zt, QStringLiteral("tab_zoom"));
    const PageTarget* onPlace = targetById(zt, QStringLiteral("tab_place"));
    const PageTarget* zd = targetById(zt, QStringLiteral("zd_val"));
    QVERIFY(onZoom);
    QVERIFY(onPlace);
    QVERIFY(zd);
    QVERIFY(!onZoom->interactive);
    QVERIFY(onZoom->actions.isEmpty());
    QVERIFY(onPlace->interactive);
    QCOMPARE(zd->pageId, QStringLiteral("main_settings_zoom"));
    QVERIFY(onZoom->geom.visual.center().y() < zd->geom.visual.center().y());
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
