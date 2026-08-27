#include "input/KeyGlyphs.h"
#include "input/KeyStateManager.h"

#include <QtTest>

using namespace gazer;

class KeyStateManagerTest final : public QObject {
    Q_OBJECT

private slots:
    void aliasesNormalizeToSlots();
    void cycleUpDownLockedUp();
    void standardKeyReleasesOneShot();
    void lockedSurvivesStandardKey();
    void severalOneShotsReleaseTogether();
    void explicitDownUpDoNotLock();
    void comboReleasesOneShotAndRestoresLocked();
    void releaseAllClearsLocked();
    void injectFailureLeavesState();
    void shiftGlyphsAndLabels();
    void strokeUsesOemAndExtraShift();
    void shiftedPunctuationTapInjectsOem();
    void comboMapsOemWithoutLetterShift();
    void extraShiftLivesInSlot();
    void extraShiftInjectFailureRollsBack();
    void holdReleasesAfterDuration();
};

namespace {

struct Event {
    QString key;
    bool down = false;
};

void wireMgr(KeyStateManager& mgr, QVector<Event>* log, bool* failNext = nullptr)
{
    mgr.setInjector([log, failNext](const QString& key, bool down, QString* error) {
        if (failNext && *failNext) {
            *failNext = false;
            if (error) {
                *error = QStringLiteral("inject failed");
            }
            return false;
        }
        log->push_back({key, down});
        return true;
    });
}

QStringList names(const QVector<Event>& log)
{
    QStringList out;
    for (const Event& e : log) {
        out.push_back(QStringLiteral("%1%2").arg(e.key, e.down ? QStringLiteral("↓")
                                                               : QStringLiteral("↑")));
    }
    return out;
}

} // namespace

void KeyStateManagerTest::aliasesNormalizeToSlots()
{
    QCOMPARE(KeyStateManager::canonicalModifier(QStringLiteral("LeftCtrl")),
             QStringLiteral("ctrl"));
    QCOMPARE(KeyStateManager::canonicalModifier(QStringLiteral("RCONTROL")),
             QStringLiteral("ctrl"));
    QCOMPARE(KeyStateManager::canonicalModifier(QStringLiteral("l-shift")),
             QStringLiteral("shift"));
    QCOMPARE(KeyStateManager::canonicalModifier(QStringLiteral("LWin")), QStringLiteral("win"));
    QCOMPARE(KeyStateManager::canonicalModifier(QStringLiteral("Menu")), QStringLiteral("alt"));
    QVERIFY(!KeyStateManager::isModifier(QStringLiteral("A")));
    QVERIFY(!KeyStateManager::isModifier(QStringLiteral("Enter")));
    QVERIFY(!KeyStateManager::isModifier(QStringLiteral("AltGr")));
}

void KeyStateManagerTest::cycleUpDownLockedUp()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);

    QVERIFY(mgr.cycle(QStringLiteral("Control")));
    QCOMPARE(mgr.state(QStringLiteral("ctrl")), KeyHoldState::Down);
    QVERIFY(mgr.isHeld(QStringLiteral("leftCtrl")));
    QVERIFY(!mgr.isLocked(QStringLiteral("Control")));
    QCOMPARE(names(log), QStringList{QStringLiteral("Control↓")});

    QVERIFY(mgr.cycle(QStringLiteral("Control")));
    QCOMPARE(mgr.state(QStringLiteral("Control")), KeyHoldState::LockedDown);
    QVERIFY(mgr.isLocked(QStringLiteral("ctrl")));
    QCOMPARE(log.size(), 1); // already down; lock is logical only

    QVERIFY(mgr.cycle(QStringLiteral("Control")));
    QCOMPARE(mgr.state(QStringLiteral("Control")), KeyHoldState::Up);
    QVERIFY(!mgr.isHeld(QStringLiteral("Control")));
    QCOMPARE(names(log), (QStringList{QStringLiteral("Control↓"), QStringLiteral("Control↑")}));
}

void KeyStateManagerTest::standardKeyReleasesOneShot()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);

    QVERIFY(mgr.activate(QStringLiteral("Shift")));
    QCOMPARE(mgr.state(QStringLiteral("Shift")), KeyHoldState::Down);

    QVERIFY(mgr.activate(QStringLiteral("a")));
    QCOMPARE(mgr.state(QStringLiteral("Shift")), KeyHoldState::Up);
    QCOMPARE(names(log),
             (QStringList{QStringLiteral("Shift↓"), QStringLiteral("a↓"), QStringLiteral("a↑"),
                          QStringLiteral("Shift↑")}));
}

void KeyStateManagerTest::lockedSurvivesStandardKey()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.cycle(QStringLiteral("Alt")));
    QVERIFY(mgr.cycle(QStringLiteral("Alt")));
    QCOMPARE(mgr.state(QStringLiteral("Alt")), KeyHoldState::LockedDown);
    log.clear();

    QVERIFY(mgr.activate(QStringLiteral("Tab")));
    QCOMPARE(mgr.state(QStringLiteral("Alt")), KeyHoldState::LockedDown);
    QCOMPARE(names(log), (QStringList{QStringLiteral("Tab↓"), QStringLiteral("Tab↑")}));
}

void KeyStateManagerTest::severalOneShotsReleaseTogether()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.activate(QStringLiteral("Control")));
    QVERIFY(mgr.activate(QStringLiteral("Alt")));
    QVERIFY(mgr.activate(QStringLiteral("c")));
    QVERIFY(!mgr.anyHeld());
    QCOMPARE(mgr.heldCanonical(), QStringList());

    QVERIFY(names(log).contains(QStringLiteral("Control↑")));
    QVERIFY(names(log).contains(QStringLiteral("Alt↑")));
    QVERIFY(names(log).contains(QStringLiteral("c↓")));
}

void KeyStateManagerTest::explicitDownUpDoNotLock()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.down(QStringLiteral("LWin")));
    QCOMPARE(mgr.state(QStringLiteral("win")), KeyHoldState::Down);
    QVERIFY(mgr.down(QStringLiteral("LWin"))); // already down
    QCOMPARE(mgr.state(QStringLiteral("win")), KeyHoldState::Down);
    QVERIFY(mgr.up(QStringLiteral("win")));
    QCOMPARE(mgr.state(QStringLiteral("LWin")), KeyHoldState::Up);
    QCOMPARE(names(log), (QStringList{QStringLiteral("LWin↓"), QStringLiteral("LWin↑")}));
}

void KeyStateManagerTest::comboReleasesOneShotAndRestoresLocked()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.cycle(QStringLiteral("Shift")));
    QVERIFY(mgr.cycle(QStringLiteral("Shift"))); // locked
    QVERIFY(mgr.cycle(QStringLiteral("Control"))); // one-shot
    log.clear();

    QVERIFY(mgr.combo({QStringLiteral("Control"), QStringLiteral("S")}));
    QCOMPARE(mgr.state(QStringLiteral("Control")), KeyHoldState::Up);
    QCOMPARE(mgr.state(QStringLiteral("Shift")), KeyHoldState::LockedDown);

    const QStringList got = names(log);
    QVERIFY(got.contains(QStringLiteral("S↓")));
    QVERIFY(got.contains(QStringLiteral("Control↑")));
    QVERIFY(got.contains(QStringLiteral("Shift↓"))); // reassert locked
}

void KeyStateManagerTest::releaseAllClearsLocked()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.cycle(QStringLiteral("Alt")));
    QVERIFY(mgr.cycle(QStringLiteral("Alt")));
    QVERIFY(mgr.releaseAll());
    QCOMPARE(mgr.state(QStringLiteral("Alt")), KeyHoldState::Up);
    QVERIFY(names(log).contains(QStringLiteral("Alt↑")));
}

void KeyStateManagerTest::injectFailureLeavesState()
{
    QVector<Event> log;
    bool failNext = true;
    KeyStateManager mgr;
    wireMgr(mgr, &log, &failNext);
    QString err;
    QVERIFY(!mgr.cycle(QStringLiteral("Control"), &err));
    QCOMPARE(mgr.state(QStringLiteral("Control")), KeyHoldState::Up);
    QCOMPARE(err, QStringLiteral("inject failed"));
    QCOMPARE(log.size(), 0);
}

void KeyStateManagerTest::shiftGlyphsAndLabels()
{
    QCOMPARE(KeyGlyphs::shiftedGlyph(QStringLiteral("q")), QStringLiteral("Q"));
    QCOMPARE(KeyGlyphs::shiftedGlyph(QStringLiteral("1")), QStringLiteral("!"));
    QCOMPARE(KeyGlyphs::shiftedGlyph(QStringLiteral("-")), QStringLiteral("_"));
    QCOMPARE(KeyGlyphs::shiftedGlyph(QStringLiteral("[")), QStringLiteral("{"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral("q"), QStringLiteral("q"), false),
             QStringLiteral("q"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral("q"), QStringLiteral("q"), true),
             QStringLiteral("Q"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral("1"), QStringLiteral("1"), true),
             QStringLiteral("!"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral(","), QStringLiteral(","), true),
             QStringLiteral("<"));
    QCOMPARE(KeyGlyphs::strokeForSend(QStringLiteral(","), false).key, QStringLiteral("OemComma"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral("Esc"), QStringLiteral("Escape"), true),
             QStringLiteral("Esc"));
    QCOMPARE(KeyGlyphs::displayLabel(QStringLiteral("@"), QStringLiteral("@"), true),
             QStringLiteral("@"));
}

void KeyStateManagerTest::strokeUsesOemAndExtraShift()
{
    const auto hyphen = KeyGlyphs::strokeForSend(QStringLiteral("-"), false);
    QCOMPARE(hyphen.key, QStringLiteral("OemMinus"));
    QVERIFY(!hyphen.extraShift);

    const auto under = KeyGlyphs::strokeForSend(QStringLiteral("_"), false);
    QCOMPARE(under.key, QStringLiteral("OemMinus"));
    QVERIFY(under.extraShift);

    const auto underShifted = KeyGlyphs::strokeForSend(QStringLiteral("_"), true);
    QVERIFY(!underShifted.extraShift);

    const auto bang = KeyGlyphs::strokeForSend(QStringLiteral("!"), false);
    QCOMPARE(bang.key, QStringLiteral("1"));
    QVERIFY(bang.extraShift);

    QVERIFY(!KeyGlyphs::strokeForSend(QStringLiteral("S"), false).extraShift);
}

void KeyStateManagerTest::shiftedPunctuationTapInjectsOem()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.activate(QStringLiteral("-")));
    QCOMPARE(names(log), (QStringList{QStringLiteral("OemMinus↓"), QStringLiteral("OemMinus↑")}));

    log.clear();
    QVERIFY(mgr.activate(QStringLiteral("_")));
    QCOMPARE(names(log),
             (QStringList{QStringLiteral("Shift↓"), QStringLiteral("OemMinus↓"),
                          QStringLiteral("OemMinus↑"), QStringLiteral("Shift↑")}));

    log.clear();
    QVERIFY(mgr.cycle(QStringLiteral("Shift")));
    QVERIFY(mgr.activate(QStringLiteral("1")));
    QVERIFY(names(log).contains(QStringLiteral("1↓")));
    QVERIFY(names(log).contains(QStringLiteral("Shift↑")));
    QCOMPARE(mgr.state(QStringLiteral("Shift")), KeyHoldState::Up);
}

void KeyStateManagerTest::comboMapsOemWithoutLetterShift()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.combo({QStringLiteral("Control"), QStringLiteral("S")}));
    QVERIFY(names(log).contains(QStringLiteral("Control↓")));
    QVERIFY(names(log).contains(QStringLiteral("S↓")));
    QVERIFY(!names(log).contains(QStringLiteral("Shift↓")));

    log.clear();
    QVERIFY(mgr.combo({QStringLiteral(",")}));
    QCOMPARE(names(log),
             (QStringList{QStringLiteral("OemComma↓"), QStringLiteral("OemComma↑")}));
}

void KeyStateManagerTest::extraShiftLivesInSlot()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.down(QStringLiteral("_")));
    QVERIFY(mgr.isHeld(QStringLiteral("shift")));
    QCOMPARE(names(log),
             (QStringList{QStringLiteral("Shift↓"), QStringLiteral("OemMinus↓")}));

    QVERIFY(mgr.up(QStringLiteral("_")));
    QVERIFY(!mgr.isHeld(QStringLiteral("shift")));
    QCOMPARE(names(log),
             (QStringList{QStringLiteral("Shift↓"), QStringLiteral("OemMinus↓"),
                          QStringLiteral("OemMinus↑"), QStringLiteral("Shift↑")}));
}

void KeyStateManagerTest::extraShiftInjectFailureRollsBack()
{
    QVector<Event> log;
    int n = 0;
    KeyStateManager mgr;
    mgr.setInjector([&](const QString& key, bool down, QString* error) {
        ++n;
        if (n == 2) {
            if (error) {
                *error = QStringLiteral("inject failed");
            }
            return false;
        }
        log.push_back({key, down});
        return true;
    });
    QString err;
    QVERIFY(!mgr.activate(QStringLiteral("_"), &err));
    QCOMPARE(err, QStringLiteral("inject failed"));
    QVERIFY(!mgr.isHeld(QStringLiteral("shift")));
    QCOMPARE(names(log), (QStringList{QStringLiteral("Shift↓"), QStringLiteral("Shift↑")}));
}

void KeyStateManagerTest::holdReleasesAfterDuration()
{
    QVector<Event> log;
    KeyStateManager mgr;
    wireMgr(mgr, &log);
    QVERIFY(mgr.hold(QStringLiteral("a"), 25));
    QCOMPARE(names(log), QStringList{QStringLiteral("a↓")});
    QTest::qWait(80);
    QCOMPARE(names(log), (QStringList{QStringLiteral("a↓"), QStringLiteral("a↑")}));
}

QObject* createKeyStateManagerTest()
{
    return new KeyStateManagerTest;
}

#include "KeyStateManagerTest.moc"
