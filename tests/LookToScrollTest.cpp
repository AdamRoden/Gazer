#include "assist/ComboMouseHit.h"
#include "assist/LtsMenu.h"
#include "assist/LtsScrollMode.h"
#include "assist/LtsSpeed.h"
#include "input/PixelScroller.h"

#include <QtTest>

using namespace gazer;

class LookToScrollTest final : public QObject {
    Q_OBJECT

private slots:
    void speedLadder();
    void falloffEaseAndHysteresis();
    void axisAccelReset();
    void scrollModeCycle();
    void pieActionsFromHit();
    void ltsRegionOrders();
    void wheelLeftoverQuantize();
    void highResWheelClassNames();
};

void LookToScrollTest::speedLadder()
{
    QCOMPARE(snapLtsSpeed(3.2), 4.0);
    QCOMPARE(snapLtsSpeed(0.7), 0.5);
    QCOMPARE(snapLtsSpeed(0.8), 1.0);
    QCOMPARE(snapLtsSpeed(2.0), 2.0);
    QCOMPARE(snapLtsSpeed(1.0), 1.0);
    QCOMPARE(snapLtsSpeed(0.5), 0.5);
    QCOMPARE(nudgeLtsSpeed(4.0, +1), 8.0);
    QCOMPARE(nudgeLtsSpeed(4.0, -1), 2.0);
    QCOMPARE(nudgeLtsSpeed(2.0, -1), 1.0);
    QCOMPARE(nudgeLtsSpeed(1.0, -1), 0.5);
    QCOMPARE(nudgeLtsSpeed(0.5, -1), 0.5);
    QCOMPARE(nudgeLtsSpeed(8.0, +1), 8.0);
    QCOMPARE(snapLtsSpeed(50.0), 8.0);
    QCOMPARE(snapLtsSpeed(20.0), 8.0);
}

void LookToScrollTest::falloffEaseAndHysteresis()
{
    QCOMPARE(easeLtsFalloff(0.0), 0.0);
    QCOMPARE(easeLtsFalloff(1.0), 1.0);
    QCOMPARE(easeLtsFalloff(0.5), 0.5);
    QVERIFY(easeLtsFalloff(0.2) > 0.19);
    QCOMPARE(ltsDeadzoneHysteresisPx(110), 27);
    QCOMPARE(ltsDeadzoneHysteresisPx(20), 16);
    QCOMPARE(ltsDeadzoneHysteresisPx(400), 40);
    QVERIFY(!ltsKeepScrolling(false, 110.0, 110));
    QVERIFY(ltsKeepScrolling(false, 110.1, 110));
    QVERIFY(ltsKeepScrolling(true, 90.0, 110));
    QVERIFY(!ltsKeepScrolling(true, 83.0, 110));
    QVERIFY(kLtsMinEngagedPxPerSec >= 12.0);
}

void LookToScrollTest::axisAccelReset()
{
    QCOMPARE(ltsDeadzoneHysteresisPx(80), 20);
    QVERIFY(!ltsAxisAccelActive(0.0, 80));
    QVERIFY(!ltsAxisAccelActive(20.0, 80));
    QVERIFY(ltsAxisAccelActive(20.1, 80));
    QVERIFY(ltsAxisAccelActive(-21.0, 80));

    double sec = 1.5;
    stepLtsAxisAccelSec(sec, false, true, 0.016);
    QCOMPARE(sec, 0.0);

    sec = 1.5;
    stepLtsAxisAccelSec(sec, true, false, 0.016);
    QCOMPARE(sec, 1.5);

    stepLtsAxisAccelSec(sec, true, true, 0.5);
    QCOMPARE(sec, 2.0);
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
             LtsMenuAction::None);
    QCOMPARE(ltsMenuActionFromHit(ComboMouseHit::Band::Drift, ComboMouseHit::Slice::Right),
             LtsMenuAction::None);
    QCOMPARE(ltsMenuActionFromHit(ComboMouseHit::Band::None, ComboMouseHit::Slice::Right),
             LtsMenuAction::None);
    QCOMPARE(QLatin1String(ltsMenuActionId(LtsMenuAction::Resume)), QLatin1String("resume"));
    QCOMPARE(ltsMenuActionFromId(QStringLiteral("cycle")), LtsMenuAction::CycleMode);
    QCOMPARE(ltsMenuActionFromId(QStringLiteral("nope")), LtsMenuAction::None);

    const char* icons[ComboMouseHit::kSliceCount] = {};
    fillLtsSliceIcons(LtsScrollMode::Horizontal, icons);
    int cycle = -1;
    int quit = -1;
    int reset = -1;
    for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
        if (kLtsSliceActions[i] == LtsMenuAction::CycleMode) {
            cycle = i;
        }
        if (kLtsSliceActions[i] == LtsMenuAction::Quit) {
            quit = i;
        }
        if (kLtsSliceActions[i] == LtsMenuAction::Reset) {
            reset = i;
        }
    }
    QVERIFY(cycle >= 0 && quit >= 0 && reset >= 0);
    QCOMPARE(QLatin1String(icons[cycle]), QLatin1String("lookToScrollHorizontal"));
    QCOMPARE(QLatin1String(icons[quit]), QLatin1String("close"));
    QCOMPARE(QLatin1String(icons[reset]), QLatin1String("cycle"));
}

void LookToScrollTest::ltsRegionOrders()
{
    const QRectF screen(0, 0, 1920, 1080);
    const double dead = 60;
    const double ring = 120;
    const double pie = 200;
    using S = ComboMouseHit::Slice;
    using A = LtsMenuAction;
    struct Case {
        QPointF o;
        double fillStart;
        bool clockwise;
        double span;
        double outer;
        S order[ComboMouseHit::kSliceCount];
        A actions[ComboMouseHit::kSliceCount];
    };
    const Case cases[] = {
        {{960, 540},
         0.0,
         true,
         360.0,
         pie,
         {S::Right, S::Move, S::Cancel, S::Drag, S::Left},
         {A::Faster, A::Reset, A::Quit, A::CycleMode, A::Slower}},
        {{0, 540},
         0.0,
         true,
         180.0,
         pie,
         {S::Left, S::Right, S::Drag, S::Move, S::Cancel},
         {A::Slower, A::Faster, A::CycleMode, A::Reset, A::Quit}},
        {{1919, 540},
         180.0,
         true,
         180.0,
         pie,
         {S::Cancel, S::Move, S::Left, S::Right, S::Drag},
         {A::Quit, A::Reset, A::Slower, A::Faster, A::CycleMode}},
        {{960, 0},
         90.0,
         true,
         180.0,
         pie,
         {S::Cancel, S::Move, S::Drag, S::Right, S::Left},
         {A::Quit, A::Reset, A::CycleMode, A::Faster, A::Slower}},
        {{960, 1079},
         270.0,
         true,
         180.0,
         pie,
         {S::Left, S::Right, S::Drag, S::Move, S::Cancel},
         {A::Slower, A::Faster, A::CycleMode, A::Reset, A::Quit}},
        {{0, 0},
         90.0,
         true,
         90.0,
         pie * 2.0,
         {S::Cancel, S::Move, S::Drag, S::Right, S::Left},
         {A::Quit, A::Reset, A::CycleMode, A::Faster, A::Slower}},
        {{1919, 0},
         180.0,
         true,
         90.0,
         pie * 2.0,
         {S::Cancel, S::Move, S::Drag, S::Right, S::Left},
         {A::Quit, A::Reset, A::CycleMode, A::Faster, A::Slower}},
        {{1919, 1079},
         270.0,
         true,
         90.0,
         pie * 2.0,
         {S::Left, S::Right, S::Drag, S::Move, S::Cancel},
         {A::Slower, A::Faster, A::CycleMode, A::Reset, A::Quit}},
        {{0, 1079},
         0.0,
         true,
         90.0,
         pie * 2.0,
         {S::Left, S::Right, S::Drag, S::Move, S::Cancel},
         {A::Slower, A::Faster, A::CycleMode, A::Reset, A::Quit}},
    };
    for (const auto& c : cases) {
        const auto L = makeLtsLayout(c.o, screen, dead, ring, pie);
        QCOMPARE(L.arcSpanDeg, c.span);
        QCOMPARE(L.pieOuter, c.outer);
        QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
        const double sliceDeg = c.span / double(ComboMouseHit::kSliceCount);
        const double r = (L.ringOuter + L.pieOuter) * 0.5;
        for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
            const double cw = c.clockwise ? c.fillStart + (double(i) + 0.5) * sliceDeg
                                          : c.fillStart - (double(i) + 0.5) * sliceDeg;
            const auto h = ComboMouseHit::hit(ComboMouseHit::pointOnRay(c.o, cw, r), c.o, L);
            QCOMPARE(h.band, ComboMouseHit::Band::Slice);
            QCOMPARE(h.slice, c.order[i]);
            QCOMPARE(ltsMenuActionFromHit(h.band, h.slice), c.actions[i]);
        }
    }
}

void LookToScrollTest::wheelLeftoverQuantize()
{
    QCOMPARE(kWheelUnitsPerNotch, 120);
    // 80 px / notch → 1.5 wheel units per px.
    QCOMPARE(120.0 / PixelScroller::kPixelsPerNotch, 1.5);

    double rem = 80.0; // exactly one notch
    QCOMPARE(takeWheelUnits(rem, 1), 120);
    QCOMPARE(rem, 0.0);

    rem = 0.4; // 0.6 units — hold until a whole unit
    QCOMPARE(takeWheelUnits(rem, 1), 0);
    rem = 0.8; // 1.2 units
    QCOMPARE(takeWheelUnits(rem, 1), 1);
    QVERIFY(qAbs(rem - (0.2 / 1.5)) < 1e-9);

    rem = -0.8;
    QCOMPARE(takeWheelUnits(rem, 1), -1);
    QVERIFY(qAbs(rem - (-0.2 / 1.5)) < 1e-9);
}

void LookToScrollTest::highResWheelClassNames()
{
    const QString yes[] = {QStringLiteral("Chrome_WidgetWin_1"),
                           QStringLiteral("Chrome_RenderWidgetHostHWND"),
                           QStringLiteral("chrome_widgetwin_0"),
                           QStringLiteral("MozillaWindowClass"),
                           QStringLiteral("MozillaCompositorWindowClass"),
                           QStringLiteral("IEFrame"),
                           QStringLiteral("Internet Explorer_Server")};
    const QString no[] = {QStringLiteral("Intermediate D3D Window"), QStringLiteral("Scintilla"),
                          QStringLiteral("SysListView32"), QStringLiteral("ScrollBar"),
                          QStringLiteral("ApplicationFrameWindow")};
    for (const QString& c : yes) {
        QVERIFY2(classLooksLikeHighResWheel(c), qPrintable(c));
    }
    for (const QString& c : no) {
        QVERIFY2(!classLooksLikeHighResWheel(c), qPrintable(c));
    }
}

QObject* createLookToScrollTest()
{
    return new LookToScrollTest;
}

#include "LookToScrollTest.moc"
