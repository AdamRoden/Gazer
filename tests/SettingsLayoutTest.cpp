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
    const QStringList ids = {QStringLiteral("main_settings_button_timing"),
                             QStringLiteral("main_settings_pointer_timing"),
                             QStringLiteral("main_settings_styles"),
                             QStringLiteral("main_settings_assist"),
                             QStringLiteral("main_settings_overlays"),
                             QStringLiteral("main_settings_theme"),
                             QStringLiteral("main_settings_speed_advanced")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        QVERIFY2(loadLayout(id, doc, &err), qPrintable(err));
        QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
        QCOMPARE(doc.grids[0].desktopMode, true);
        QCOMPARE(PageDimParse::token(doc.grids[0].size.x), QStringLiteral("A_ScreenHeight/9*16"));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 7);
    QCOMPARE(tabs->cells.size(), 7);
    int tabRoles = 0;
    bool hasDone = false;
    for (const PageCell& c : tabs->cells) {
        if (c.role == QLatin1String("tab")) {
            ++tabRoles;
        }
        if (c.id == QLatin1String("tab_done")) {
            hasDone = true;
            QVERIFY(c.isInteractive());
        }
    }
    QCOMPARE(tabRoles, 6);
    QVERIFY(hasDone);
}

void SettingsLayoutTest::timingSectionUsesRowWeights()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
    const PageGrid* sec = doc.findGrid(QStringLiteral("sec_timing"));
    QVERIFY(sec);
    QCOMPARE(sec->styleId, QStringLiteral("group"));
    QCOMPARE(sec->rows, 5);
    QCOMPARE(sec->rowWeights, QVector<double>({1.0, 2.0, 2.0, 2.0, 2.0}));
    const PageGrid* row = doc.findGrid(QStringLiteral("row_scan"));
    QVERIFY(row);
    QCOMPARE(row->styleId, QStringLiteral("row"));
    QCOMPARE(row->rowSpan, 1);
    QCOMPARE(row->row, 2);

    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* header = targetById(t, QStringLiteral("h_timing"));
    const PageTarget* desc = targetById(t, QStringLiteral("scan_label"));
    QVERIFY(header);
    QVERIFY(desc);
    QVERIFY(qAbs(2.0 * header->geom.visual.height() - desc->geom.visual.height()) < 1.5);
}

void SettingsLayoutTest::stepperWidths()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* val = targetById(t, QStringLiteral("scan_val"));
    QVERIFY(val);
    QCOMPARE(val->role, QStringLiteral("value"));
    QCOMPARE(val->settingKey, QStringLiteral("scanGraceMs"));
}

void SettingsLayoutTest::ltsHasNoMaxSpeedOrPlaceCursor()
{
    QFile f(layoutPath(QStringLiteral("main_settings_overlays")));
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_overlays"), doc, &err), qPrintable(err));
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
    const QStringList pages = {QStringLiteral("main_settings_button_timing"),
                               QStringLiteral("main_settings_pointer_timing"),
                               QStringLiteral("main_settings_styles"),
                               QStringLiteral("main_settings_assist"),
                               QStringLiteral("main_settings_overlays"),
                               QStringLiteral("main_settings_theme")};
    QSet<QString> opened;
    const PageCell* done = doc.findCell(QStringLiteral("done"));
    QVERIFY(done);
    QVERIFY(done->isInteractive());
    for (const PageGrid& g : doc.grids) {
        for (const PageCell& c : g.cells) {
            for (const PageAction& a : c.actions) {
                if (a.type == PageActionType::Nav && a.verb == PageVerb::Open
                    && a.targetKind == PageTargetKind::Page) {
                    opened.insert(a.targetId);
                }
            }
        }
    }
    for (const QString& id : pages) {
        QVERIFY2(opened.contains(id), qPrintable(id));
    }
    QCOMPARE(opened.size(), 6);
}

void SettingsLayoutTest::presetsComeFirst()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
    const PageGrid* presets = doc.findGrid(QStringLiteral("sec_presets"));
    const PageGrid* timing = doc.findGrid(QStringLiteral("sec_timing"));
    QVERIFY(presets);
    QVERIFY(timing);
    QVERIFY(presets->row < timing->row);
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_pointer_timing"), pointer, &err),
             qPrintable(err));
    QVERIFY2(loadLayout(QStringLiteral("main_settings_styles"), look, &err), qPrintable(err));
    PageDocument speed;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), speed, &err),
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
    QVERIFY2(loadLayout(QStringLiteral("main_settings_styles"), doc, &err), qPrintable(err));
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
    const PageGrid* schemesSec = doc.findGrid(QStringLiteral("sec_schemes"));
    const PageGrid* progressSec = doc.findGrid(QStringLiteral("sec_progress"));
    const PageGrid* optionsSec = doc.findGrid(QStringLiteral("sec_options"));
    QVERIFY(appearanceSec);
    QVERIFY(schemesSec);
    QVERIFY(progressSec);
    QVERIFY(optionsSec);
    QVERIFY(appearanceSec->row < schemesSec->row);
    QVERIFY(schemesSec->row < progressSec->row);
    QVERIFY(progressSec->row < optionsSec->row);
    QCOMPARE(doc.findCell(QStringLiteral("h_schemes"))->label, QStringLiteral("Accent"));
    QCOMPARE(doc.findGrid(QStringLiteral("row_schemes"))->columns, 9);
    QCOMPARE(doc.findGrid(QStringLiteral("row_progress"))->columns, 9);
    QCOMPARE(doc.findCell(QStringLiteral("pri_0"))->label, QStringLiteral("Red"));
    QCOMPARE(doc.findCell(QStringLiteral("pri_5"))->label, QStringLiteral("Blue"));
    QCOMPARE(doc.findCell(QStringLiteral("sec_8"))->label, QStringLiteral("Pink"));
    for (int i = 0; i < 9; ++i) {
        const PageCell* pri = doc.findCell(QStringLiteral("pri_%1").arg(i));
        QVERIFY2(pri, qPrintable(QStringLiteral("pri_%1").arg(i)));
        QCOMPARE(pri->role, QStringLiteral("choice"));
        QVERIFY(pri->isInteractive());
        QVERIFY(!doc.findCell(QStringLiteral("pri_sel_%1").arg(i)));
    }
    QVERIFY(!doc.findCell(QStringLiteral("pri_9")));
    for (int i = 0; i < 9; ++i) {
        const PageCell* sec = doc.findCell(QStringLiteral("sec_%1").arg(i));
        QVERIFY2(sec, qPrintable(QStringLiteral("sec_%1").arg(i)));
        QCOMPARE(sec->role, QStringLiteral("swatch"));
        QVERIFY(sec->isInteractive());
        QVERIFY(!doc.findCell(QStringLiteral("sec_sel_%1").arg(i)));
    }
    QVERIFY(!doc.findCell(QStringLiteral("sec_9")));
    QVERIFY(doc.findCell(QStringLiteral("sat_dec")));
    QVERIFY(doc.findCell(QStringLiteral("sat_inc")));
    QVERIFY(!doc.findCell(QStringLiteral("sat_val")));
    QVERIFY(doc.findCell(QStringLiteral("edit_colors")));
    QVERIFY(!doc.findCell(QStringLiteral("fl_sw")));
}

void SettingsLayoutTest::moreOpensAdvanced()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadLayout(QStringLiteral("main_settings_button_timing"), doc, &err), qPrintable(err));
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
    QCOMPARE(tabs->columns, 7);
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
