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
    QCOMPARE(tabs->columns, 8);
    QCOMPARE(tabs->cells.size(), 8);
    int tabRoles = 0;
    bool hasDone = false;
    bool hasSpeech = false;
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
    }
    QCOMPARE(tabRoles, 7);
    QVERIFY(hasDone);
    QVERIFY(hasSpeech);
}

void SettingsLayoutTest::timingSectionUsesRowWeights()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* daily = doc.findGrid(QStringLiteral("sec_daily"));
    QVERIFY(daily);
    QCOMPARE(daily->styleId, QStringLiteral("group"));
    QCOMPARE(daily->rows, 3);
    QCOMPARE(daily->rowWeights, QVector<double>({1.0, 2.0, 2.0}));
    const PageGrid* designer = doc.findGrid(QStringLiteral("sec_designer"));
    QVERIFY(designer);
    QCOMPARE(designer->rows, 3);
    QCOMPARE(designer->rowWeights, QVector<double>({1.0, 2.0, 2.0}));
    const PageGrid* row = doc.findGrid(QStringLiteral("row_daily_scan"));
    QVERIFY(row);
    QCOMPARE(row->styleId, QStringLiteral("row"));
    QCOMPARE(row->rowSpan, 1);
    QCOMPARE(row->row, 2);

    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* header = targetById(t, QStringLiteral("h_daily"));
    const PageTarget* desc = targetById(t, QStringLiteral("dd_scan_label"));
    QVERIFY(header);
    QVERIFY(desc);
    QVERIFY(qAbs(2.0 * header->geom.visual.height() - desc->geom.visual.height()) < 1.5);
}

void SettingsLayoutTest::stepperWidths()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* act = doc.findGrid(QStringLiteral("act_daily_scan"));
    QVERIFY(act);
    QCOMPARE(act->gapPx, 0);
    QCOMPARE(act->columns, 9);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* dec = targetById(t, QStringLiteral("dd_scan_dec"));
    const PageTarget* val = targetById(t, QStringLiteral("dd_scan_val"));
    const PageTarget* inc = targetById(t, QStringLiteral("dd_scan_inc"));
    const PageTarget* edit = targetById(t, QStringLiteral("dd_scan_edit"));
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
    const PageTarget* val = targetById(t, QStringLiteral("dd_scan_val"));
    QVERIFY(val);
    QCOMPARE(val->role, QStringLiteral("value"));
    QCOMPARE(val->settingKey, QStringLiteral("dailyScanGraceMs"));
    const PageTarget* designer = targetById(t, QStringLiteral("scan_val"));
    QVERIFY(designer);
    QCOMPARE(designer->settingKey, QStringLiteral("scanGraceMs"));
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
                               QStringLiteral("main_settings_speech")};
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
    QCOMPARE(opened.size(), 7);
    PageDocument speech;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speech"), speech, &err), qPrintable(err));
    QCOMPARE(speech.findCell(QStringLiteral("speed_value"))->settingKey,
             QStringLiteral("speechSpeed"));
    QCOMPARE(speech.findCell(QStringLiteral("volume_value"))->settingKey,
             QStringLiteral("speechVolume"));
    QVERIFY(speech.findCell(QStringLiteral("tab_speech")));
}

void SettingsLayoutTest::presetsComeFirst()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speed"), doc, &err), qPrintable(err));
    const PageGrid* presets = doc.findGrid(QStringLiteral("sec_presets"));
    const PageGrid* daily = doc.findGrid(QStringLiteral("sec_daily"));
    const PageGrid* designer = doc.findGrid(QStringLiteral("sec_designer"));
    QVERIFY(presets);
    QVERIFY(daily);
    QVERIFY(designer);
    QVERIFY(presets->row < daily->row);
    QVERIFY(daily->row < designer->row);
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
    const PageCell* light = doc.findCell(QStringLiteral("light"));
    QVERIFY(light);
    QCOMPARE(light->role, QStringLiteral("choice"));
    QCOMPARE(doc.findCell(QStringLiteral("light"))->col, 0);
    QVERIFY(doc.findCell(QStringLiteral("light_tint")));
    QVERIFY(doc.findCell(QStringLiteral("dark_tint")));
    QVERIFY(doc.findCell(QStringLiteral("dark")));
    QVERIFY(!doc.findCell(QStringLiteral("custom")));
    QVERIFY(!doc.findCell(QStringLiteral("scheme_blue")));
    QVERIFY(!doc.findCell(QStringLiteral("c_more")));
    const PageGrid* appearanceSec = doc.findGrid(QStringLiteral("sec_appearance"));
    const PageGrid* selectedSec = doc.findGrid(QStringLiteral("sec_selected"));
    const PageGrid* palPrimary = doc.findGrid(QStringLiteral("row_pal_primary"));
    QVERIFY(appearanceSec);
    QVERIFY(selectedSec);
    QVERIFY(palPrimary);
    QVERIFY(!doc.findGrid(QStringLiteral("sec_palettes")));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_options")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pal_analogous2")));
    QVERIFY(appearanceSec->row < selectedSec->row);
    QVERIFY(selectedSec->row < palPrimary->row);
    QVERIFY(doc.findGrid(QStringLiteral("row_pal_triadic2"))->row
            < doc.findGrid(QStringLiteral("row_hue"))->row);
    QVERIFY(doc.findGrid(QStringLiteral("row_hue")));
    QVERIFY(doc.findCell(QStringLiteral("track_h")));
    QCOMPARE(doc.findCell(QStringLiteral("track_h"))->role, QStringLiteral("slider"));
    QVERIFY(!doc.findCell(QStringLiteral("track_h"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("track_h"))->actions.isEmpty());
    QVERIFY(doc.findCell(QStringLiteral("edit_h"))->isInteractive());
    QCOMPARE(doc.findCell(QStringLiteral("edit_h"))->actions[0].command,
             QStringLiteral("settings.color.scrub.h"));
    QVERIFY(doc.findCell(QStringLiteral("dec_h"))->isInteractive());
    QCOMPARE(doc.findCell(QStringLiteral("dec_h"))->actions[0].command,
             QStringLiteral("settings.color.nudge.h.dec"));
    QVERIFY(doc.findCell(QStringLiteral("inc_h"))->isInteractive());
    QCOMPARE(doc.findCell(QStringLiteral("inc_h"))->actions[0].command,
             QStringLiteral("settings.color.nudge.h.inc"));
    QVERIFY(!doc.findCell(QStringLiteral("track_s"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("edit_s"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("dec_s"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("inc_s"))->isInteractive());
    QVERIFY(!doc.findCell(QStringLiteral("track_l"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("edit_l"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("dec_l"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("inc_l"))->isInteractive());
    QCOMPARE(doc.findGrid(QStringLiteral("row_pal_analogous1"))->row + 1,
             doc.findGrid(QStringLiteral("row_pal_triadic1"))->row);
    QVERIFY(!doc.findCell(QStringLiteral("sp_mid2")));
    QVERIFY(doc.findCell(QStringLiteral("hex"))->isInteractive());
    QVERIFY(!doc.findGrid(QStringLiteral("sec_schemes")));
    QVERIFY(!doc.findGrid(QStringLiteral("sec_progress")));
    QVERIFY(!doc.findCell(QStringLiteral("pri_0")));
    QVERIFY(!doc.findCell(QStringLiteral("sec_8")));
    QVERIFY(!doc.findCell(QStringLiteral("page_title")));
    QVERIFY(!doc.findCell(QStringLiteral("h_appearance")));
    QVERIFY(!doc.findCell(QStringLiteral("edit_colors")));
    QVERIFY(!doc.findCell(QStringLiteral("pick_source")));
    QVERIFY(!doc.findCell(QStringLiteral("sat_dec")));
    QVERIFY(!doc.findCell(QStringLiteral("assign_p")));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_primary"))->label, QStringLiteral("Primary"));
    QCOMPARE(doc.findCell(QStringLiteral("h_pal_complementary"))->label,
             QStringLiteral("Complementary"));
    QCOMPARE(palPrimary->columns, 12);
    QCOMPARE(palPrimary->gapPx, 0);
    QVERIFY(!doc.findCell(QStringLiteral("source_swatch")));
    QCOMPARE(doc.findCell(QStringLiteral("p_swatch"))->role, QStringLiteral("choice"));
    QCOMPARE(doc.findCell(QStringLiteral("s_swatch"))->role, QStringLiteral("choice"));
    for (int i = 0; i < 10; ++i) {
        const PageCell* shade = doc.findCell(QStringLiteral("pal_primary_%1").arg(i));
        QVERIFY2(shade, qPrintable(QStringLiteral("pal_primary_%1").arg(i)));
        QVERIFY(shade->isInteractive());
        QVERIFY(shade->label.isEmpty());
    }
    QVERIFY(doc.findCell(QStringLiteral("pal_analogous1_0")));
    QVERIFY(!doc.findCell(QStringLiteral("pal_analogous2_0")));
    QVERIFY(doc.findCell(QStringLiteral("pal_triadic2_0")));
    QVERIFY(!doc.findCell(QStringLiteral("fl_sw")));
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
    QCOMPARE(doc.findCell(QStringLiteral("fl_sw"))->settingKey, QStringLiteral("flashColor"));
    QCOMPARE(doc.findCell(QStringLiteral("fl_fg"))->role, QStringLiteral("choice"));
    QCOMPARE(doc.findCell(QStringLiteral("grace_val"))->settingKey, QStringLiteral("dwellGraceMs"));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 8);
    QVERIFY(doc.findCell(QStringLiteral("tab_speech")));
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
