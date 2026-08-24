#include "layout/PageHit.h"
#include "layout/PageLoader.h"

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

} // namespace

class SettingsLayoutTest final : public QObject {
    Q_OBJECT

private slots:
    void pagesAnchorTop();
    void tabsEqualWidth();
    void timingSectionUsesRowWeights();
    void stepperWidths();
    void valueLabelKeepsKey();
};

void SettingsLayoutTest::pagesAnchorTop()
{
    const QStringList ids = {QStringLiteral("main_settings_button_timing"),
                             QStringLiteral("main_settings_pointer_timing"),
                             QStringLiteral("main_settings_styles"),
                             QStringLiteral("main_settings_assist"),
                             QStringLiteral("main_settings_lts"),
                             QStringLiteral("main_settings_theme")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                             + QStringLiteral("/resources/layouts/") + id + QStringLiteral(".xml");
        QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
        QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
        QVERIFY(!doc.grids[0].style.background.has_value());
        QVERIFY(doc.styles.contains(QStringLiteral("plain")));
        QVERIFY(doc.styles.contains(QStringLiteral("join")));
    }
}

void SettingsLayoutTest::tabsEqualWidth()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/main_settings_button_timing.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
    QVERIFY(tabs);
    QCOMPARE(tabs->columns, 6);
    QCOMPARE(tabs->cells.size(), 6);
}

void SettingsLayoutTest::timingSectionUsesRowWeights()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/main_settings_button_timing.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    const PageGrid* sec = doc.findGrid(QStringLiteral("sec_timing"));
    QVERIFY(sec);
    QCOMPARE(sec->styleId, QStringLiteral("group"));
    QCOMPARE(sec->rows, 4);
    QCOMPARE(sec->rowWeights, QVector<double>({1.0, 2.0, 2.0, 2.0}));
    const PageGrid* row = doc.findGrid(QStringLiteral("row_grace"));
    QVERIFY(row);
    QCOMPARE(row->styleId, QStringLiteral("row"));
    QCOMPARE(row->rowSpan, 1);
    QCOMPARE(row->row, 1);

    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* header = targetById(t, QStringLiteral("h_timing"));
    const PageTarget* desc = targetById(t, QStringLiteral("grace_label"));
    QVERIFY(header);
    QVERIFY(desc);
    QVERIFY(qAbs(2.0 * header->geom.visual.height() - desc->geom.visual.height()) < 1.5);
}

void SettingsLayoutTest::stepperWidths()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/main_settings_button_timing.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    const PageGrid* act = doc.findGrid(QStringLiteral("act_grace"));
    QVERIFY(act);
    QCOMPARE(act->gapPx, 0);
    QCOMPARE(act->columns, 7);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* dec = targetById(t, QStringLiteral("grace_dec"));
    const PageTarget* val = targetById(t, QStringLiteral("grace_val"));
    const PageTarget* inc = targetById(t, QStringLiteral("grace_inc"));
    const PageTarget* edit = targetById(t, QStringLiteral("grace_edit"));
    QVERIFY(dec && val && inc && edit);
    QCOMPARE(val->clusterSlot, QStringLiteral("value"));
    QVERIFY(dec->geom.visual.width() < edit->geom.visual.width());
    QVERIFY(edit->geom.visual.width() < val->geom.visual.width());
    QCOMPARE(dec->geom.visual.width(), inc->geom.visual.width());
    QVERIFY(qAbs(dec->geom.visual.right() - val->geom.visual.left()) < 0.75);
}

void SettingsLayoutTest::valueLabelKeepsKey()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/main_settings_button_timing.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* val = targetById(t, QStringLiteral("grace_val"));
    QVERIFY(val);
    QCOMPARE(val->role, QStringLiteral("label"));
    QCOMPARE(val->settingKey, QStringLiteral("dwellGraceMs"));
}

QObject* createSettingsLayoutTest()
{
    return new SettingsLayoutTest;
}

#include "SettingsLayoutTest.moc"
