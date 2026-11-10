#include "layout/PageCompose.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageResolve.h"

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

bool paintsOwnShape(const QString& role)
{
    const QString r = role.toLower();
    return r == QLatin1String("swatch") || r == QLatin1String("swatchrect")
           || r == QLatin1String("scrollbar") || r == QLatin1String("curvefield")
           || r == QLatin1String("headpreview") || r == QLatin1String("label")
           || r == QLatin1String("value") || r == QLatin1String("display");
}

bool nearPx(double a, double b)
{
    return qAbs(a - b) < 0.01;
}

QString buttonCornerProblem(const PageDocument& doc, const PageGrid& grid)
{
    for (const PageCell& cell : grid.cells) {
        if (paintsOwnShape(cell.role) || (cell.actions.isEmpty() && cell.phases.isEmpty())) {
            continue;
        }
        const QString style = cell.styleId;
        const bool allowed = style == QLatin1String("button") || style == QLatin1String("join")
                             || style == QLatin1String("joinLeft")
                             || style == QLatin1String("joinRight");
        if (!allowed) {
            return doc.id + QStringLiteral(" ") + cell.id + QStringLiteral(" style='") + style
                   + QStringLiteral("'");
        }
        const PageBox radius = PageResolve::style(doc, style, cell.style).resolvedRadius();
        const bool left = style == QLatin1String("joinLeft");
        const bool right = style == QLatin1String("joinRight");
        const bool mid = style == QLatin1String("join");
        const double expectTl = mid ? 0.0 : (left ? 10.0 : (right ? 0.0 : 10.0));
        const double expectTr = mid ? 0.0 : (right ? 10.0 : (left ? 0.0 : 10.0));
        const double expectBr = mid ? 0.0 : (right ? 10.0 : (left ? 0.0 : 10.0));
        const double expectBl = mid ? 0.0 : (left ? 10.0 : (right ? 0.0 : 10.0));
        if (!nearPx(radius.at(0), expectTl) || !nearPx(radius.at(1), expectTr)
            || !nearPx(radius.at(2), expectBr) || !nearPx(radius.at(3), expectBl)) {
            return doc.id + QStringLiteral(" ") + cell.id + QStringLiteral(" radius ")
                   + radius.toToken();
        }
    }
    for (const PageGrid& child : grid.subGrids) {
        const QString problem = buttonCornerProblem(doc, child);
        if (!problem.isEmpty()) {
            return problem;
        }
    }
    return {};
}

} // namespace

class SettingsLayoutTest final : public QObject {
    Q_OBJECT

private slots:
    void pagesAnchorTop();
    void settingsHostUsesSidebar();
    void timingRowsStayCompact();
    void stepperWidths();
    void valueLabelKeepsKey();
    void ltsHasNoMaxSpeedOrPlaceCursor();
    void overlayIdsAreUnique();
    void speakAndHeadBoards();
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
    void buttonCornersMatch();
};

void SettingsLayoutTest::pagesAnchorTop()
{
    const QStringList ids = {QStringLiteral("main_settings_dwell"),
                             QStringLiteral("main_settings_pointer"),
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
        const PageCell* done = doc.findCell(QStringLiteral("tab_done"));
        QVERIFY(done);
        QCOMPARE(done->row, 0);
        QCOMPARE(done->col, 1);
        QCOMPARE(done->label, QStringLiteral("Close"));
        QVERIFY(done->isInteractive());
        QVERIFY(!doc.grids[0].rowTracks.isEmpty());
        QCOMPARE(PageDimParse::token(doc.grids[0].rowTracks[0]), QStringLiteral("96"));
        QCOMPARE(doc.styles.value(QStringLiteral("group")).resolvedRadius().first(), 16.0);
        QCOMPARE(doc.styles.value(QStringLiteral("row")).resolvedThickness().first(), 0.0);
    }
}

void SettingsLayoutTest::settingsHostUsesSidebar()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_host"), doc, &err), qPrintable(err));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(doc.findGrid(QStringLiteral("body")));
    QCOMPARE(doc.findGrid(QStringLiteral("body"))->src, QStringLiteral("main_settings_dwell"));
    QCOMPARE(doc.findGrid(QStringLiteral("body"))->row, 0);
    QCOMPARE(doc.findGrid(QStringLiteral("body"))->col, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("body"))->rowSpan, 2);
    QCOMPARE(PageDimParse::token(doc.grids[0].rowTracks[0]), QStringLiteral("96"));
    QCOMPARE(doc.findCell(QStringLiteral("host_title"))->row, 0);
    QCOMPARE(doc.findCell(QStringLiteral("host_title"))->col, 0);
    QVERIFY(!doc.findCell(QStringLiteral("tab_done")));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 1);
    QCOMPARE(tabs->rows, 8);
    QCOMPARE(tabs->cells.size(), 7);
    QCOMPARE(PageDimParse::token(doc.grids[0].columnTracks[0]), QStringLiteral("248"));
    int tabRoles = 0;
    for (const PageCell& c : tabs->cells) {
        QCOMPARE(c.role, QStringLiteral("tab"));
        QCOMPARE(c.col, 0);
        ++tabRoles;
    }
    QCOMPARE(tabRoles, 7);
    QCOMPARE(doc.findCell(QStringLiteral("host_title"))->label, QStringLiteral("Settings"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_dwell"))->actions[0].targetId,
             QStringLiteral("main_settings_dwell"));
    QCOMPARE(doc.findCell(QStringLiteral("tab_pointer"))->actions[0].targetId,
             QStringLiteral("main_settings_pointer"));
    QVERIFY(!doc.findCell(QStringLiteral("tab_place")));
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
    QVERIFY(!doc.findCell(QStringLiteral("tab_look")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_advanced")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_speed")));
    QVERIFY(!doc.findCell(QStringLiteral("tab_assist")));
}

void SettingsLayoutTest::timingRowsStayCompact()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->rows, 2);
    QCOMPARE(PageDimParse::tokenList(doc.grids[0].rowTracks), QStringLiteral("96,*"));
    const PageGrid* scroller = doc.findGrid(QStringLiteral("scroller"));
    QVERIFY(scroller);
    QVERIFY(scroller->scroll);
    QCOMPARE(scroller->rows, 5);
    QCOMPARE(PageDimParse::tokenList(scroller->rowTracks),
             QStringLiteral("272,272,168,272,168"));
    const PageGrid* dwell = doc.findGrid(QStringLiteral("sec_dwell"));
    QVERIFY(dwell);
    QCOMPARE(dwell->styleId, QStringLiteral("group"));
    QCOMPARE(dwell->rows, 3);
    QCOMPARE(dwell->row, 1);
    QCOMPARE(PageDimParse::tokenList(dwell->rowTracks), QStringLiteral("48,96,96"));
    const PageGrid* standard = doc.findGrid(QStringLiteral("row_dwell"));
    QVERIFY(standard);
    QCOMPARE(standard->styleId, QStringLiteral("row"));
    QCOMPARE(standard->rowSpan, 1);
    QCOMPARE(standard->row, 1);
    const PageGrid* boost = doc.findGrid(QStringLiteral("row_rapid_dwell"));
    QVERIFY(boost);
    QCOMPARE(boost->row, 2);
    const PageGrid* keys = doc.findGrid(QStringLiteral("sec_keys"));
    QVERIFY(keys);
    QCOMPARE(keys->rows, 2);
    QCOMPARE(keys->row, 2);
    QCOMPARE(PageDimParse::tokenList(keys->rowTracks), QStringLiteral("48,96"));

    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* header = targetById(t, QStringLiteral("h_dwell"));
    const PageTarget* desc = targetById(t, QStringLiteral("rapid_dwell_label"));
    const PageTarget* gravity = targetById(t, QStringLiteral("gravity_label"));
    QVERIFY(header);
    QVERIFY(desc);
    QVERIFY(gravity);
    QVERIFY(qAbs(header->geom.visual.height() - 48.0) < 1.5);
    QVERIFY(qAbs(desc->geom.visual.height() - 96.0) < 1.5);
    QVERIFY(qAbs(gravity->geom.visual.height() - 96.0) < 1.5);
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
    QVERIFY(seen.contains(QStringLiteral("combo_on")));
    QVERIFY(seen.contains(QStringLiteral("row_combo_style")));
    QVERIFY(!seen.contains(QStringLiteral("splash_on")));
    QVERIFY(!seen.contains(QStringLiteral("pd_val")));
}

void SettingsLayoutTest::speakAndHeadBoards()
{
    QString err;
    PageDocument speak;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_speak"), speak, &err), qPrintable(err));
    QCOMPARE(speak.findCell(QStringLiteral("speed_value"))->settingKey,
             QStringLiteral("speechSpeed"));
    QCOMPARE(speak.findCell(QStringLiteral("pitch_value"))->settingKey,
             QStringLiteral("speechPitch"));
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
    QCOMPARE(doc.findCell(QStringLiteral("h_dwell"))->label, QStringLiteral("Sequences"));
    QCOMPARE(doc.findCell(QStringLiteral("p_label"))->caption,
             QStringLiteral("Slow, Normal, Fast, or Custom. Also sets pointer dwell, zoom dwell, "
                            "and blink grace."));
    QCOMPARE(doc.findGrid(QStringLiteral("row_dwell"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_rapid_dwell"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_keys"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("row_key_gravity"))->row, 1);
    QVERIFY(!doc.findGrid(QStringLiteral("sec_fine")));
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
    PageDocument gaze;
    PageDocument dwell;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_pointer"), zoom, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), gaze, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), dwell, &err), qPrintable(err));
    QCOMPARE(zoom.findCell(QStringLiteral("page_title"))->label, QStringLiteral("Pointer"));
    QCOMPARE(gaze.findCell(QStringLiteral("page_title"))->label, QStringLiteral("Gaze"));
    QCOMPARE(zoom.findCell(QStringLiteral("zl_val"))->settingKey, QStringLiteral("pickZoom"));
    QCOMPARE(zoom.findCell(QStringLiteral("zs_val"))->settingKey, QStringLiteral("pickWindowPx"));
    QCOMPARE(zoom.findCell(QStringLiteral("zd_val"))->settingKey, QStringLiteral("magPickDwellMs"));
    QVERIFY(!zoom.findCell(QStringLiteral("fsd_val")));
    QVERIFY(!zoom.findCell(QStringLiteral("fsh_val")));
    QVERIFY(!zoom.findCell(QStringLiteral("zi_cur")));
    QVERIFY(!zoom.findCell(QStringLiteral("zi_dot")));
    QVERIFY(!zoom.findCell(QStringLiteral("zi_xh")));
    QVERIFY(!zoom.findCell(QStringLiteral("zi_gz")));
    QCOMPARE(zoom.findGrid(QStringLiteral("sec_magnify"))->rows, 8);
    QVERIFY(zoom.findCell(QStringLiteral("test_move")));
    QVERIFY(!zoom.findCell(QStringLiteral("zm_none")));
    QCOMPARE(zoom.findCell(QStringLiteral("zm_pre"))->role, QStringLiteral("toggle"));
    QVERIFY(!zoom.findCell(QStringLiteral("zm_fs")));
    QVERIFY(!zoom.findCell(QStringLiteral("zm_fs2")));
    QVERIFY(!dwell.findCell(QStringLiteral("zd_val")));
    QVERIFY(!gaze.findCell(QStringLiteral("zl_val")));
    QVERIFY(!gaze.findCell(QStringLiteral("test_move")));
    QVERIFY(zoom.findGrid(QStringLiteral("scroller"))->scroll);
    QCOMPARE(zoom.findGrid(QStringLiteral("scroller"))->columns, 1);
    QCOMPARE(zoom.findGrid(QStringLiteral("scroller"))->rows, 2);
    QCOMPARE(PageDimParse::tokenList(zoom.findGrid(QStringLiteral("scroller"))->rowTracks),
             QStringLiteral("584,792"));
    QVERIFY(zoom.findGrid(QStringLiteral("page_scroll")));
    QVERIFY(!zoom.findGrid(QStringLiteral("cols")));
    QVERIFY(!zoom.findGrid(QStringLiteral("sec_combo")));
    QVERIFY(!zoom.findGrid(QStringLiteral("sec_zoom")));
    QVERIFY(!zoom.findGrid(QStringLiteral("sec_window")));
    QVERIFY(!zoom.findCell(QStringLiteral("combo_on")));
    QCOMPARE(zoom.findGrid(QStringLiteral("sec_place"))->row, 0);
    QCOMPARE(zoom.findGrid(QStringLiteral("sec_magnify"))->row, 1);
    QVERIFY(gaze.findGrid(QStringLiteral("scroller"))->scroll);
    QCOMPARE(gaze.findGrid(QStringLiteral("scroller"))->rows, 4);
    QCOMPARE(PageDimParse::tokenList(gaze.findGrid(QStringLiteral("scroller"))->rowTracks),
             QStringLiteral("376,376,480,584"));
    QVERIFY(!gaze.findGrid(QStringLiteral("col_gaze")));
    QVERIFY(!gaze.findGrid(QStringLiteral("col_look")));
    QCOMPARE(gaze.findGrid(QStringLiteral("sec_follow"))->row, 0);
    QCOMPARE(gaze.findGrid(QStringLiteral("sec_lens"))->row, 1);
    QCOMPARE(gaze.findGrid(QStringLiteral("sec_look"))->row, 2);
    QCOMPARE(gaze.findCell(QStringLiteral("look_hint"))->row, 0);
    QCOMPARE(gaze.findCell(QStringLiteral("look_hint"))->textStyle, QStringLiteral("caption"));
    QCOMPARE(gaze.findGrid(QStringLiteral("row_mouse"))->columns, 2);
    QCOMPARE(gaze.findGrid(QStringLiteral("act_mouse"))->columns, 2);
    QCOMPARE(gaze.findGrid(QStringLiteral("act_mouse"))->gapPx, 0);
    QCOMPARE(gaze.findGrid(QStringLiteral("sec_combo"))->row, 3);
    QCOMPARE(zoom.findCell(QStringLiteral("pd_val"))->settingKey,
             QStringLiteral("mouseMoveDwellMs"));
    QCOMPARE(zoom.findCell(QStringLiteral("pg_val"))->settingKey,
             QStringLiteral("mouseMoveSelectTimeoutMs"));
    QVERIFY(zoom.findCell(QStringLiteral("pi_cur")));
    QVERIFY(zoom.findCell(QStringLiteral("pi_dot")));
    QVERIFY(zoom.findCell(QStringLiteral("pi_xh")));
    QVERIFY(zoom.findCell(QStringLiteral("test_move")));
    QCOMPARE(zoom.findCell(QStringLiteral("test_label"))->label, QStringLiteral("Try"));
    QCOMPARE(zoom.findGrid(QStringLiteral("row_test"))->row, 1);
    QCOMPARE(zoom.findGrid(QStringLiteral("row_mode"))->columns, 2);
    QCOMPARE(zoom.findCell(QStringLiteral("zs_full"))->col, 1);
    QCOMPARE(zoom.findCell(QStringLiteral("zs_full"))->styleId, QStringLiteral("button"));
    QCOMPARE(zoom.findCell(QStringLiteral("zs_full_l"))->caption,
             QStringLiteral("Use the short side of the screen."));
    QVERIFY(!zoom.findCell(QStringLiteral("zs_label"))->caption.contains(
        QStringLiteral("full"), Qt::CaseInsensitive));
    QCOMPARE(zoom.findGrid(QStringLiteral("act_zs"))->columns, 9);
    QVERIFY(zoom.findCell(QStringLiteral("zs_edit")));
    QCOMPARE(gaze.findCell(QStringLiteral("combo_on_l"))->label, QStringLiteral("Try"));
    QCOMPARE(gaze.findGrid(QStringLiteral("row_combo_on"))->row, 1);
    QCOMPARE(gaze.findGrid(QStringLiteral("row_combo_inner"))->row, 2);
    QVERIFY(gaze.findCell(QStringLiteral("combo_on")));
    QCOMPARE(zoom.findCell(QStringLiteral("pp_r"))->role, QStringLiteral("toggle"));
    QCOMPARE(zoom.findGrid(QStringLiteral("row_pg"))->row, 3);
    QCOMPARE(zoom.findGrid(QStringLiteral("row_pp"))->row, 4);
    QCOMPARE(zoom.findGrid(QStringLiteral("act_pp"))->columns, 3);
    QCOMPARE(zoom.findCell(QStringLiteral("pp_label"))->caption,
             QStringLiteral("What you see while placing the cursor."));
    QVERIFY(!zoom.findCell(QStringLiteral("bp_r")));
    QVERIFY(!dwell.findCell(QStringLiteral("pp_r")));
    QVERIFY(!gaze.findCell(QStringLiteral("test_move")));
    QVERIFY(!gaze.findCell(QStringLiteral("zl_val")));
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
    QVERIFY(!doc.findGrid(QStringLiteral("theme_columns")));
    const PageGrid* scroller = doc.findGrid(QStringLiteral("scroller"));
    const PageGrid* surfaces = doc.findGrid(QStringLiteral("sec_surfaces"));
    const PageGrid* accents = doc.findGrid(QStringLiteral("sec_accents"));
    QVERIFY(scroller);
    QVERIFY(scroller->scroll);
    QCOMPARE(scroller->columns, 1);
    QVERIFY(surfaces);
    QVERIFY(accents);
    QCOMPARE(surfaces->col, 0);
    QCOMPARE(surfaces->row, 0);
    QCOMPARE(accents->col, 0);
    QCOMPARE(accents->row, 1);
    QVERIFY(doc.findGrid(QStringLiteral("page_scroll")));
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
    QVERIFY(!doc.findCell(QStringLiteral("pp_r")));
    QCOMPARE(doc.findGrid(QStringLiteral("act_bp"))->columns, 3);
    QVERIFY2(loadLayout(QStringLiteral("main_settings_pointer"), doc, &err), qPrintable(err));
    QVERIFY(!doc.findCell(QStringLiteral("pp_b")));
    QCOMPARE(doc.findGrid(QStringLiteral("act_pp"))->columns, 3);
}

void SettingsLayoutTest::dwellKeepsGraceOnPage()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_dwell"), doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->rows, 2);
    QVERIFY(doc.findGrid(QStringLiteral("scroller"))->scroll);
    QVERIFY(!doc.findCell(QStringLiteral("open_advanced")));
    QVERIFY(!doc.findCell(QStringLiteral("pd_val")));
    QVERIFY(!doc.findCell(QStringLiteral("zd_val")));
    QVERIFY(!doc.findCell(QStringLiteral("pp_r")));
    QVERIFY(!doc.findGrid(QStringLiteral("row_pp")));
    QCOMPARE(doc.findCell(QStringLiteral("scan_val"))->settingKey, QStringLiteral("scanGraceMs"));
    QCOMPARE(doc.findCell(QStringLiteral("grace_val"))->settingKey, QStringLiteral("dwellGraceMs"));
    QCOMPARE(doc.findGrid(QStringLiteral("row_scan"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("row_grace"))->row, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_grace"))->rows, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_grace"))->row, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("row_bp"))->row, 1);
    QCOMPARE(doc.findGrid(QStringLiteral("sec_progress"))->rows, 2);
    QCOMPARE(doc.findGrid(QStringLiteral("act_bp"))->columns, 3);
    QCOMPARE(doc.findCell(QStringLiteral("bp_r"))->role, QStringLiteral("toggle"));
    QCOMPARE(doc.findCell(QStringLiteral("h_prog"))->label, QStringLiteral("Button progress"));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_pointer"), zoom, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_setup"), setup, &err), qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_gaze"), gaze, &err), qPrintable(err));
    QCOMPARE(zoom.findCell(QStringLiteral("zd_val"))->settingKey,
             QStringLiteral("magPickDwellMs"));
    QVERIFY(!zoom.findCell(QStringLiteral("fsh_val")));
    QVERIFY(!zoom.findCell(QStringLiteral("aci_val")));
    QVERIFY(!zoom.findCell(QStringLiteral("splash_on")));
    QCOMPARE(setup.findCell(QStringLiteral("aci_val"))->settingKey,
             QStringLiteral("layoutAutoCloseIdleMs"));
    QCOMPARE(setup.findCell(QStringLiteral("ac_on"))->role, QStringLiteral("toggle"));
    QCOMPARE(setup.findGrid(QStringLiteral("board"))->rows, 6);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_session"))->row, 5);
    QCOMPARE(setup.findGrid(QStringLiteral("sec_session"))->rows, 4);
    QCOMPARE(setup.findCell(QStringLiteral("log_open"))->actions[0].command,
             QStringLiteral("openLogFolder"));
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
    QVERIFY(doc.findCell(QStringLiteral("combo_on")));
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
    QCOMPARE(speak.findCell(QStringLiteral("key_edit"))->label, QStringLiteral("Edit"));
    QCOMPARE(speak.findCell(QStringLiteral("key_edit"))->actions[0].command,
             QStringLiteral("settings.speech.editKey"));
    QVERIFY(!speak.findCell(QStringLiteral("key_link")));
    QVERIFY(speak.findGrid(QStringLiteral("scroller"))->scroll);
    QCOMPARE(speak.findGrid(QStringLiteral("scroller"))->rows, 2);
    QCOMPARE(speak.findGrid(QStringLiteral("sec_speech"))->row, 0);
    QCOMPARE(speak.findGrid(QStringLiteral("sec_speech"))->rows, 7);
    QCOMPARE(speak.findGrid(QStringLiteral("row_key"))->row, 1);
    QCOMPARE(speak.findGrid(QStringLiteral("row_eng"))->row, 2);
    QCOMPARE(speak.findGrid(QStringLiteral("row_spd"))->row, 3);
    QCOMPARE(speak.findGrid(QStringLiteral("row_pitch"))->row, 4);
    QCOMPARE(speak.findGrid(QStringLiteral("row_vol"))->row, 5);
    QCOMPARE(speak.findGrid(QStringLiteral("row_predict"))->row, 6);
    QCOMPARE(speak.findCell(QStringLiteral("pit_edit"))->actions[0].command,
             QStringLiteral("settings.edit.speechPitch"));
    QCOMPARE(speak.findCell(QStringLiteral("spd_edit"))->actions[0].command,
             QStringLiteral("settings.edit.speechSpeed"));
    QCOMPARE(speak.findCell(QStringLiteral("vol_edit"))->actions[0].command,
             QStringLiteral("settings.edit.speechVolume"));
    QCOMPARE(speak.findCell(QStringLiteral("predict_on"))->role, QStringLiteral("toggle"));
    QCOMPARE(speak.findCell(QStringLiteral("predict_on"))->actions[0].command,
             QStringLiteral("settings.speech.predictions.toggle"));
    QVERIFY(speak.findCell(QStringLiteral("m_sapi")));
    QVERIFY(speak.findCell(QStringLiteral("m_turbo")));
    QCOMPARE(speak.findCell(QStringLiteral("m_turbo"))->actions[0].command,
             QStringLiteral("settings.speech.model.eleven_v4_turbo"));
    QCOMPARE(speak.findGrid(QStringLiteral("sec_output"))->row, 1);
    QCOMPARE(speak.findGrid(QStringLiteral("sec_output"))->col, 0);
    QCOMPARE(speak.grids[0].columns, 2);
    QCOMPARE(PageDimParse::tokenList(speak.grids[0].columnTracks), QStringLiteral("*,72"));
    QVERIFY(speak.findGrid(QStringLiteral("page_scroll")));
    QCOMPARE(speak.findCell(QStringLiteral("set_scroll_up"))->actions[0].command,
             QStringLiteral("settings.scroll.up"));
    QCOMPARE(speak.findCell(QStringLiteral("set_scroll_down"))->actions[0].command,
             QStringLiteral("settings.scroll.down"));
    QVERIFY(speak.findGrid(QStringLiteral("out_list")));
    QCOMPARE(speak.findCell(QStringLiteral("h_output"))->label, QStringLiteral("Output"));
    QCOMPARE(speak.findCell(QStringLiteral("out_refresh"))->actions[0].command,
             QStringLiteral("settings.speech.output.refresh"));
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
    const PageTarget* tabZoom = targetById(t, QStringLiteral("tab_pointer"));
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
    QVERIFY(tab->geom.visual.right() < dwell->geom.visual.left());
    QVERIFY(tab->geom.visual.center().y() < dwell->geom.visual.center().y());
    const PageTarget* hitTab = PageHit::at(t, tabZoom->geom.visual.center(), 1.0, {}, grids);
    QVERIFY(hitTab);
    QCOMPARE(hitTab->id, QStringLiteral("tab_pointer"));
    const PageTarget* hitBody = PageHit::at(t, dwell->geom.visual.center(), 1.0, {}, grids);
    QVERIFY(hitBody);
    QCOMPARE(hitBody->id, QStringLiteral("dwell_edit"));

    PageDocument zoom;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_pointer"), zoom, &err), qPrintable(err));
    PageGrid* body = host.findGrid(QStringLiteral("body"));
    QVERIFY(body);
    body->src = QStringLiteral("main_settings_pointer");
    QHash<QString, PageDocument> zoomed;
    zoomed.insert(zoom.id, zoom);
    const auto zoomPtrs = PageCompose::pointers(zoomed);
    const QVector<PageTarget> zt =
        PageHit::collect(host, frame, {}, false, nullptr, false, std::nullopt, &zoomPtrs);
    const PageTarget* onZoom = targetById(zt, QStringLiteral("tab_pointer"));
    const PageTarget* onGaze = targetById(zt, QStringLiteral("tab_gaze"));
    const PageTarget* zd = targetById(zt, QStringLiteral("zd_val"));
    QVERIFY(onZoom);
    QVERIFY(onGaze);
    QVERIFY(zd);
    QVERIFY(!targetById(zt, QStringLiteral("tab_place")));
    QVERIFY(!onZoom->interactive);
    QVERIFY(onZoom->actions.isEmpty());
    QVERIFY(onGaze->interactive);
    QCOMPARE(zd->pageId, QStringLiteral("main_settings_pointer"));
    QVERIFY(onZoom->geom.visual.right() < zd->geom.visual.left());
    QVERIFY(onZoom->geom.visual.center().y() < zd->geom.visual.center().y());
}

void SettingsLayoutTest::buttonCornersMatch()
{
    const QStringList ids = {QStringLiteral("main_settings_host"),
                             QStringLiteral("main_settings_dwell"),
                             QStringLiteral("main_settings_pointer"),
                             QStringLiteral("main_settings_gaze"),
                             QStringLiteral("main_settings_head_pose"),
                             QStringLiteral("main_settings_speak"),
                             QStringLiteral("main_settings_theme"),
                             QStringLiteral("main_settings_setup")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        QVERIFY2(loadLayout(id, doc, &err), qPrintable(err));
        QVERIFY(doc.styles.contains(QStringLiteral("button")));
        QCOMPARE(doc.styles.value(QStringLiteral("button")).radius->toToken(),
                 QStringLiteral("10"));
        for (const PageGrid& grid : doc.grids) {
            const QString problem = buttonCornerProblem(doc, grid);
            QVERIFY2(problem.isEmpty(), qPrintable(problem));
        }
    }
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
