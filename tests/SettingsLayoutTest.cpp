#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"

#include <QFile>
#include <QSet>
#include <QStringList>
#include <QtTest>

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
    void hubOpensSixBoards();
    void presetsComeFirst();
    void zoomLivesOnPointerNotLook();
    void choiceAndToggleRoles();
    void themeHasFlashModes();
    void moreOpensAdvanced();
    void advancedHasHoldAndAutoclose();
};

void SettingsLayoutTest::pagesAnchorTop()
{
    const QStringList ids = {QStringLiteral("main_settings_speed"),
                             QStringLiteral("main_settings_magnify"),
                             QStringLiteral("main_settings_indicators"),
                             QStringLiteral("main_settings_assist"),
                             QStringLiteral("main_settings_tools"),
                             QStringLiteral("main_settings_theme"),
                             QStringLiteral("main_settings_speech"),
                             QStringLiteral("main_settings_head_pose"),
                             QStringLiteral("main_settings_speed_advanced")};
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
        QVERIFY(doc.styles.contains(QStringLiteral("tabbar")));
        QCOMPARE(doc.styles.value(QStringLiteral("group")).resolvedRadius().first(), 16.0);
        QCOMPARE(doc.styles.value(QStringLiteral("row")).resolvedThickness().first(), 0.0);
    }
}

void SettingsLayoutTest::tabsEqualWidth()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 9);
    QCOMPARE(tabs->cells.size(), 9);
    int tabRoles = 0;
    bool hasDone = false;
    bool hasSpeech = false;
    bool hasHead = false;
    for (const PageCell& c : tabs->cells) {
        if (c.role == QLatin1String("tab")) {
            ++tabRoles;
        }
        if (c.id == QLatin1String("tab_done")) {
            hasDone = true;
            QVERIFY(c.isInteractive());
        }
        if (c.id == QLatin1String("tab_speech")) {
            hasSpeech = true;
        }
        if (c.id == QLatin1String("tab_head")) {
            hasHead = true;
        }
    }
    QCOMPARE(tabRoles, 8);
    QVERIFY(hasDone);
    QVERIFY(hasSpeech);
    QVERIFY(hasHead);
}

void SettingsLayoutTest::timingSectionUsesRowWeights()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* dwell = doc.findGrid(QStringLiteral("sec_dwell"));
    QVERIFY(dwell);
    QCOMPARE(dwell->styleId, QStringLiteral("group"));
    QCOMPARE(dwell->rows, 3);
    QCOMPARE(PageDimParse::tokenList(dwell->rowTracks), QStringLiteral("*,2*,2*"));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed_advanced"), doc, &err),
             qPrintable(err));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
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
    QVERIFY(!targetById(t, QStringLiteral("scan_val")));
    QVERIFY(!targetById(t, QStringLiteral("dd_scan_val")));
}

void SettingsLayoutTest::ltsHasNoMaxSpeedOrPlaceCursor()
{
    QFile f(layoutPath(QStringLiteral("main_settings_tools")));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_tools"), doc, &err), qPrintable(err));
    const QStringList ids = PageEdit::allIds(doc);
    QSet<QString> seen;
    for (const QString& id : ids) {
        QVERIFY2(!seen.contains(id), qPrintable(id));
        seen.insert(id);
    }
    QVERIFY(seen.contains(QStringLiteral("row_ind")));
    QVERIFY(seen.contains(QStringLiteral("row_combo_style")));
    QVERIFY(seen.contains(QStringLiteral("combo_on")));
    QVERIFY(seen.contains(QStringLiteral("lts_on")));
    QVERIFY(seen.contains(QStringLiteral("page_title")));
}

void SettingsLayoutTest::hubOpensSixBoards()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings"), doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("main_settings"));
    QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
    QCOMPARE(PageDimParse::token(doc.grids[0].size.x),
             QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
    QCOMPARE(PageDimParse::token(doc.grids[0].size.y), QStringLiteral("A_ScreenHeight"));
    const QStringList pages = {QStringLiteral("main_settings_speed"),
                               QStringLiteral("main_settings_magnify"),
                               QStringLiteral("main_settings_indicators"),
                               QStringLiteral("main_settings_assist"),
                               QStringLiteral("main_settings_tools"),
                               QStringLiteral("main_settings_theme"),
                               QStringLiteral("main_settings_speech"),
                               QStringLiteral("main_settings_head_pose")};
    QSet<QString> opened;
    const PageCell* done = doc.findCell(QStringLiteral("done"));
    QVERIFY(done);
    QVERIFY(done->isInteractive());
    for (const PageGrid& g : doc.grids) {
        for (const PageCell& c : g.cells) {
            for (const PageAction& a : c.actions) {
                if (a.type == PageActionType::Nav && a.verb == PageVerb::Open) {
                    opened.insert(a.targetId);
                }
            }
        }
    }
    for (const QString& id : pages) {
        QVERIFY2(opened.contains(id), qPrintable(id));
    }
    QCOMPARE(opened.size(), 8);
    PageDocument speech;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speech"), speech, &err), qPrintable(err));
    QCOMPARE(speech.findCell(QStringLiteral("speed_value"))->settingKey,
             QStringLiteral("speechSpeed"));
    QCOMPARE(speech.findCell(QStringLiteral("volume_value"))->settingKey,
             QStringLiteral("speechVolume"));
    QVERIFY(speech.findCell(QStringLiteral("tab_speech")));
    PageDocument head;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_head_pose"), head, &err), qPrintable(err));
    QVERIFY(head.findCell(QStringLiteral("head_preview")));
    QCOMPARE(head.findCell(QStringLiteral("head_preview"))->role, QStringLiteral("headpreview"));
    QVERIFY(head.findCell(QStringLiteral("pose_curve")));
    QCOMPARE(head.findCell(QStringLiteral("pose_curve"))->role, QStringLiteral("curvefield"));
    QVERIFY(!head.findCell(QStringLiteral("pose_curve"))->isInteractive());
    QVERIFY(head.findCell(QStringLiteral("axis_yaw")));
    QVERIFY(head.findCell(QStringLiteral("axis_pitch")));
    QVERIFY(head.findCell(QStringLiteral("axis_roll")));
    QVERIFY(head.findCell(QStringLiteral("axis_x")));
    QVERIFY(head.findCell(QStringLiteral("axis_y")));
    QVERIFY(head.findCell(QStringLiteral("axis_z")));
    QCOMPARE(head.findCell(QStringLiteral("axis_yaw"))->role, QStringLiteral("choice"));
}

void SettingsLayoutTest::presetsComeFirst()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* presets = doc.findGrid(QStringLiteral("sec_presets"));
    const PageGrid* dwell = doc.findGrid(QStringLiteral("sec_dwell"));
    QVERIFY(presets);
    QVERIFY(dwell);
    QVERIFY(presets->row < dwell->row);
    QCOMPARE(doc.findGrid(QStringLiteral("row_dwell"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_rapid_dwell"))->row, 2);
    QCOMPARE(doc.findCell(QStringLiteral("dwell_label"))->label, QStringLiteral("Standard"));
    QCOMPARE(doc.findCell(QStringLiteral("rapid_dwell_label"))->label, QStringLiteral("Rapid"));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_rapid")));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_designer")));
    const PageCell* slow = doc.findCell(QStringLiteral("p_slow"));
    QVERIFY(slow);
    QCOMPARE(slow->role, QStringLiteral("choice"));
    QCOMPARE(doc.findCell(QStringLiteral("p_custom"))->role, QStringLiteral("choice"));
    QVERIFY(doc.findCell(QStringLiteral("p_save"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("p_restore"))->isInteractive());
}

void SettingsLayoutTest::zoomLivesOnPointerNotLook()
{
    PageDocument pointer;
    PageDocument look;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_magnify"), pointer, &err),
             qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_indicators"), look, &err), qPrintable(err));
    PageDocument speed;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), speed, &err),
             qPrintable(err));
    QVERIFY(pointer.findCell(QStringLiteral("zl_val")));
    QCOMPARE(pointer.findCell(QStringLiteral("zl_val"))->settingKey, QStringLiteral("pickZoom"));
    QVERIFY(pointer.findCell(QStringLiteral("zs_val")));
    QVERIFY(!pointer.findCell(QStringLiteral("zd_val")));
    QCOMPARE(speed.findCell(QStringLiteral("zd_val"))->settingKey, QStringLiteral("magPickDwellMs"));
    QVERIFY(look.findCell(QStringLiteral("test_move")));
    QVERIFY(pointer.findCell(QStringLiteral("test_move")));
    QVERIFY(!pointer.findCell(QStringLiteral("zm_none")));
    QCOMPARE(pointer.findCell(QStringLiteral("zm_pre"))->role, QStringLiteral("toggle"));
    QCOMPARE(pointer.findCell(QStringLiteral("zm_fs"))->role, QStringLiteral("toggle"));
    QCOMPARE(pointer.findCell(QStringLiteral("zm_fs2"))->role, QStringLiteral("toggle"));
    QVERIFY(!look.findCell(QStringLiteral("zl_val")));
    QVERIFY(!look.findGrid(QStringLiteral("sec_zw")));
}

void SettingsLayoutTest::choiceAndToggleRoles()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_indicators"), doc, &err), qPrintable(err));
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
    QCOMPARE(doc.findCell(QStringLiteral("h_surfaces"))->label,
             QStringLiteral("Background and Surface"));
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
    QCOMPARE(surfaces->rows, 5);
    const PageGrid* actFlash = doc.findGrid(QStringLiteral("act_flash"));
    QVERIFY(actFlash);
    QCOMPARE(actFlash->row, 4);
    QCOMPARE(actFlash->columns, 3);
    const PageCell* flFg = doc.findCell(QStringLiteral("fl_fg"));
    const PageCell* flCustom = doc.findCell(QStringLiteral("fl_custom"));
    const PageCell* flSw = doc.findCell(QStringLiteral("fl_sw"));
    QVERIFY(flFg);
    QVERIFY(flCustom);
    QVERIFY(flSw);
    QCOMPARE(flFg->role, QStringLiteral("choice"));
    QCOMPARE(flCustom->role, QStringLiteral("choice"));
    QCOMPARE(flSw->role, QStringLiteral("swatch"));
    QCOMPARE(flSw->settingKey, QStringLiteral("flashColor"));
    QCOMPARE(flFg->actions[0].command, QStringLiteral("settings.flash.foreground"));
    QCOMPARE(flCustom->actions[0].command, QStringLiteral("settings.flash.custom"));
    QCOMPARE(flSw->actions[0].command, QStringLiteral("settings.edit.color.flashColor"));
}

void SettingsLayoutTest::moreOpensAdvanced()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageCell* more = doc.findCell(QStringLiteral("open_advanced"));
    QVERIFY(more);
    QVERIFY(more->isInteractive());
    bool opens = false;
    for (const PageAction& a : more->actions) {
        if (a.type == PageActionType::Nav && a.verb == PageVerb::Open
            && a.targetId == QLatin1String("main_settings_speed_advanced")) {
            opens = true;
        }
    }
    QVERIFY(opens);
}

void SettingsLayoutTest::advancedHasHoldAndAutoclose()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed_advanced"), doc, &err), qPrintable(err));
    QCOMPARE(doc.findCell(QStringLiteral("fsh_val"))->settingKey,
             QStringLiteral("mouseMoveForesightHoldMs"));
    QCOMPARE(doc.findCell(QStringLiteral("aci_val"))->settingKey,
             QStringLiteral("layoutAutoCloseIdleMs"));
    QCOMPARE(doc.findCell(QStringLiteral("ac_on"))->role, QStringLiteral("toggle"));
    QCOMPARE(doc.findCell(QStringLiteral("fd_val"))->settingKey, QStringLiteral("flashMs"));
    QVERIFY(!doc.findCell(QStringLiteral("fl_sw")));
    QVERIFY(!doc.findCell(QStringLiteral("fl_fg")));
    QVERIFY(!doc.findCell(QStringLiteral("fl_custom")));
    const PageGrid* board = doc.findGrid(QStringLiteral("board"));
    QVERIFY(board);
    QCOMPARE(board->rows, 5);
    const PageGrid* session = doc.findGrid(QStringLiteral("sec_session"));
    QVERIFY(session);
    QCOMPARE(session->rows, 4);
    QCOMPARE(session->row, 4);
    QCOMPARE(session->rowSpan, 1);
    QCOMPARE(doc.findCell(QStringLiteral("grace_val"))->settingKey, QStringLiteral("dwellGraceMs"));
    QCOMPARE(doc.findCell(QStringLiteral("scan_val"))->settingKey, QStringLiteral("scanGraceMs"));
    QCOMPARE(doc.findGrid(QStringLiteral("row_scan"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("row_pg"))->row, 3);
    const PageGrid* grace = doc.findGrid(QStringLiteral("sec_grace"));
    QVERIFY(grace);
    QCOMPARE(grace->rows, 4);
    QCOMPARE(grace->row, 2);
    QCOMPARE(grace->rowSpan, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_fs"))->row, 3);
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 9);
    QVERIFY(doc.findCell(QStringLiteral("tab_speech")));
    QVERIFY(doc.findCell(QStringLiteral("tab_head")));
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
