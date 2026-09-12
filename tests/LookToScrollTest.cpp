#include "assist/ComboMouseHit.h"
#include "assist/LtsMenu.h"
#include "assist/LtsScrollMode.h"
#include "assist/LtsSpeed.h"

#include <QtTest>

using namespace gazer;

class LookToScrollTest final : public QObject {
    Q_OBJECT

private slots:
    void speedLadder();
    void falloffEaseAndHysteresis();
    void scrollModeCycle();
    void pieActionsFromHit();
};

void LookToScrollTest::speedLadder()
{
    QCOMPARE(snapLtsSpeed(4.4), 5.0);
    QCOMPARE(snapLtsSpeed(2.0), 1.0);
    QCOMPARE(snapLtsSpeed(4.0), 5.0);
    QCOMPARE(snapLtsSpeed(1.0), 1.0);
    QCOMPARE(nudgeLtsSpeed(5.0, +1), 10.0);
    QCOMPARE(nudgeLtsSpeed(5.0, -1), 1.0);
    QCOMPARE(nudgeLtsSpeed(1.0, -1), 1.0);
    QCOMPARE(nudgeLtsSpeed(40.0, +1), 40.0);
    QCOMPARE(nudgeLtsSpeed(20.0, +1), 40.0);
    QCOMPARE(snapLtsSpeed(50.0), 40.0);
}

void LookToScrollTest::falloffEaseAndHysteresis()
{
    QCOMPARE(easeLtsFalloff(0.0), 0.0);
    QCOMPARE(easeLtsFalloff(1.0), 1.0);
    QCOMPARE(easeLtsFalloff(0.5), 0.25);
    QVERIFY(easeLtsFalloff(0.2) > 0.03);
    QCOMPARE(ltsDeadzoneHysteresisPx(110), 27);
    QCOMPARE(ltsDeadzoneHysteresisPx(20), 16);
    QCOMPARE(ltsDeadzoneHysteresisPx(400), 40);
    QVERIFY(!ltsKeepScrolling(false, 110.0, 110));
    QVERIFY(ltsKeepScrolling(false, 110.1, 110));
    QVERIFY(ltsKeepScrolling(true, 90.0, 110));
    QVERIFY(!ltsKeepScrolling(true, 83.0, 110));
    QVERIFY(kLtsMinEngagedPxPerSec >= 12.0);
}

void LookToScrollTest::scrollModeCycle()
{
    QCOMPARE(cycleLtsScrollMode(LtsScrollMode::Vertical), LtsScrollMode::Horizontal);
    QCOMPARE(cycleLtsScrollMode(LtsScrollMode::Horizontal), LtsScrollMode::Both);
    QCOMPARE(cycleLtsScrollMode(LtsScrollMode::Both), LtsScrollMode::Vertical);
    QCOMPARE(ltsScrollModeFromInt(-1), LtsScrollMode::Vertical);
    QCOMPARE(ltsScrollModeFromInt(99), LtsScrollMode::Both);
    QCOMPARE(QLatin1String(ltsScrollModeIcon(LtsScrollMode::Vertical)),
             QLatin1String("lookToScrollVertical"));
    QCOMPARE(QLatin1String(ltsScrollModeIcon(LtsScrollMode::Horizontal)),
             QLatin1String("lookToScrollHorizontal"));
    QCOMPARE(QLatin1String(ltsScrollModeIcon(LtsScrollMode::Both)),
             QLatin1String("lookToScroll"));

    double v = 3.0;
    double h = 4.0;
    applyLtsScrollMode(LtsScrollMode::Vertical, v, h);
    QCOMPARE(v, 3.0);
    QCOMPARE(h, 0.0);
    v = 3.0;
    h = 4.0;
    applyLtsScrollMode(LtsScrollMode::Horizontal, v, h);
    QCOMPARE(v, 0.0);
    QCOMPARE(h, 4.0);
    v = 3.0;
    h = 4.0;
    applyLtsScrollMode(LtsScrollMode::Both, v, h);
    QCOMPARE(v, 3.0);
    QCOMPARE(h, 4.0);
}

void LookToScrollTest::pieActionsFromHit()
{
    const QPointF o(0, 0);
    auto at = [&](double x, double y) {
        return ComboMouseHit::hit(QPointF(x, y), o, 40, 70, 120);
    };
    const auto top = at(0, -100);
    QCOMPARE(ltsMenuActionFromHit(top.band, top.slice), LtsMenuAction::Faster);
    const auto right = at(100, 40);
    QCOMPARE(ltsMenuActionFromHit(right.band, right.slice), LtsMenuAction::Reset);
    const auto bottom = at(0, 100);
    QCOMPARE(ltsMenuActionFromHit(bottom.band, bottom.slice), LtsMenuAction::Quit);
    const auto left = at(-100, 0);
    QCOMPARE(ltsMenuActionFromHit(left.band, left.slice), LtsMenuAction::CycleMode);
    const auto topLeft = at(-70, -70);
    QCOMPARE(ltsMenuActionFromHit(topLeft.band, topLeft.slice), LtsMenuAction::Slower);
    QCOMPARE(ltsMenuActionFromHit(ComboMouseHit::Band::Deadzone, ComboMouseHit::Slice::Right),
             LtsMenuAction::Resume);
    QCOMPARE(ltsMenuActionFromHit(ComboMouseHit::Band::Drift, ComboMouseHit::Slice::Right),
             LtsMenuAction::Resume);
    QCOMPARE(ltsMenuActionFromHit(ComboMouseHit::Band::None, ComboMouseHit::Slice::Right),
             LtsMenuAction::None);
    QCOMPARE(QLatin1String(ltsMenuActionId(LtsMenuAction::Resume)), QLatin1String("resume"));
    QCOMPARE(ltsMenuActionFromId(QStringLiteral("cycle")), LtsMenuAction::CycleMode);
    QCOMPARE(ltsMenuActionFromId(QStringLiteral("nope")), LtsMenuAction::None);

    const char* icons[ComboMouseHit::kSliceCount] = {};
    fillLtsSliceIcons(LtsScrollMode::Horizontal, icons);
    int cycle = -1;
    int quit = -1;
    for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
        if (kLtsSliceActions[i] == LtsMenuAction::CycleMode) {
            cycle = i;
        }
        if (kLtsSliceActions[i] == LtsMenuAction::Quit) {
            quit = i;
        }
    }
    QVERIFY(cycle >= 0 && quit >= 0);
    QCOMPARE(QLatin1String(icons[cycle]), QLatin1String("lookToScrollHorizontal"));
    QCOMPARE(QLatin1String(icons[quit]), QLatin1String("close"));
}

QObject* createLookToScrollTest()
{
    return new LookToScrollTest;
}

#include "LookToScrollTest.moc"
