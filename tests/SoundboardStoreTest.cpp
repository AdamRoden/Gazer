#include "assist/SoundboardStore.h"
#include "assist/SpeechHistory.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

class SoundboardStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void missingFileCreatesStarters();
    void unknownFormatKeepsStarters();
    void roundTrip();
    void rejectBadClipId();
    void assignCreateAndOverwrite();
    void playPolicy();
    void importAndEvictUnreferenced();
    void assignGrowsRows();
    void overwriteDeletesOldClip();
    void importEvictsOldestHistoryFirst();
    void removeButtonAndSetColor();
    void removeAndStyleTopic();
};

QString writeBytes(const QString& path, int n)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return {};
    }
    f.write(QByteArray(n, 'M'));
    return path;
}

void SoundboardStoreTest::missingFileCreatesStarters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.topics().size() >= 3);
    QCOMPARE(s.activeTopicId(), QStringLiteral("starter-everyday"));
    QVERIFY(QFile::exists(s.boardsPath()));
    const SoundboardTopic* t = s.activeTopic();
    QVERIFY(t);
    QVERIFY(!t->buttons.isEmpty());
    QCOMPARE(t->buttons[0].utteranceText, QStringLiteral("Hello"));
}

void SoundboardStoreTest::unknownFormatKeepsStarters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = QDir(dir.path()).filePath(QStringLiteral("boards.json"));
    const QByteArray foreign = QByteArrayLiteral("{\"topics\":[]}");
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(foreign), foreign.size());
    }
    SoundboardStore s(dir.path());
    QString err;
    QVERIFY(!s.load(&err));
    QVERIFY(!err.isEmpty());
    QVERIFY(s.topics().size() >= 3);
    QCOMPARE(s.activeTopicId(), QStringLiteral("starter-everyday"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), foreign);
}

void SoundboardStoreTest::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore a(dir.path());
    QVERIFY(a.load());
    a.activeTopic()->name = QStringLiteral("Renamed");
    QVERIFY(a.save());
    SoundboardStore b(dir.path());
    QVERIFY(b.load());
    QCOMPARE(b.activeTopic()->name, QStringLiteral("Renamed"));
    QCOMPARE(b.topics().size(), a.topics().size());
}

void SoundboardStoreTest::rejectBadClipId()
{
    QVERIFY(!SoundboardStore::validClipId(QStringLiteral("../x")));
    QVERIFY(!SoundboardStore::validClipId(QStringLiteral("clips/foo")));
    QVERIFY(!SoundboardStore::validClipId({}));
    QVERIFY(SoundboardStore::validClipId(QStringLiteral("deadbeef")));
    QVERIFY(SoundboardStore::validClipId(QStringLiteral("0123456789abcdef0123456789abcdef")));
}

void SoundboardStoreTest::assignCreateAndOverwrite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.newTopic());
    QCOMPARE(s.activeTopic()->buttons.size(), 0);
    SoundboardButton seed;
    seed.label = QStringLiteral("Hi");
    seed.utteranceText = QStringLiteral("Hi there");
    seed.sourceText = QStringLiteral("Hi there");
    QVERIFY(s.assignCell(0, 0, seed));
    QCOMPARE(s.activeTopic()->buttons.size(), 1);
    QCOMPARE(s.buttonAt(*s.activeTopic(), 0, 0)->label, QStringLiteral("Hi"));
    SoundboardButton over;
    over.label = QStringLiteral("Bye");
    over.sourceText = QStringLiteral("Bye");
    QVERIFY(s.assignCell(0, 0, over));
    QCOMPARE(s.activeTopic()->buttons.size(), 1);
    QCOMPARE(s.buttonAt(*s.activeTopic(), 0, 0)->label, QStringLiteral("Bye"));
    QVERIFY(!s.assignCell(20, 0, seed));
}

void SoundboardStoreTest::playPolicy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    SoundboardButton u;
    u.utteranceText = QStringLiteral("Live");
    u.sourceText = QStringLiteral("Src");
    u.clipId = QStringLiteral("deadbeef");
    QCOMPARE(s.planPlay(u).kind, SoundboardStore::PlayPlan::Kind::Utterance);
    QCOMPARE(s.planPlay(u).text, QStringLiteral("Live"));

    SoundboardButton c;
    c.clipId = QStringLiteral("deadbeef");
    c.sourceText = QStringLiteral("Src");
    QCOMPARE(s.planPlay(c).kind, SoundboardStore::PlayPlan::Kind::Source);

    SoundboardButton none;
    QCOMPARE(s.planPlay(none).kind, SoundboardStore::PlayPlan::Kind::None);
}

void SoundboardStoreTest::importAndEvictUnreferenced()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    {
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(64, 'M'));
    }
    QString err;
    const QString id = s.importClip(src, &err);
    QVERIFY2(!id.isEmpty(), qPrintable(err));
    QVERIFY(QFile::exists(s.clipPath(id)));
    s.evictUnreferencedClips();
    QVERIFY(!QFile::exists(s.clipPath(id)));

    QVERIFY(s.newTopic());
    SoundboardButton seed;
    seed.label = QStringLiteral("Clip");
    seed.sourceText = QStringLiteral("Clip");
    seed.clipId = s.importClip(src, &err);
    QVERIFY(!seed.clipId.isEmpty());
    QVERIFY(s.assignCell(0, 1, seed));
    s.evictUnreferencedClips();
    QVERIFY(QFile::exists(s.clipPath(seed.clipId)));
}

void SoundboardStoreTest::assignGrowsRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.newTopic());
    QCOMPARE(s.activeTopic()->gridRows, 3);
    SoundboardButton seed;
    seed.label = QStringLiteral("Grow");
    seed.sourceText = QStringLiteral("Grow");
    QVERIFY(s.assignCell(3, 0, seed));
    QCOMPARE(s.activeTopic()->gridRows, 4);
    QVERIFY(s.buttonAt(*s.activeTopic(), 3, 0));
    QVERIFY(s.assignCell(5, 1, seed));
    QCOMPARE(s.activeTopic()->gridRows, 6);
    QVERIFY(!s.assignCell(6, 0, seed));
}

void SoundboardStoreTest::overwriteDeletesOldClip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.newTopic());
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    QVERIFY(!writeBytes(src, 64).isEmpty());
    QString err;
    SoundboardButton first;
    first.label = QStringLiteral("A");
    first.sourceText = QStringLiteral("A");
    first.clipId = s.importClip(src, &err);
    QVERIFY2(!first.clipId.isEmpty(), qPrintable(err));
    QVERIFY(s.assignCell(0, 0, first));
    QVERIFY(QFile::exists(s.clipPath(first.clipId)));
    SoundboardButton second;
    second.label = QStringLiteral("B");
    second.sourceText = QStringLiteral("B");
    second.clipId = s.importClip(src, &err);
    QVERIFY2(!second.clipId.isEmpty(), qPrintable(err));
    QVERIFY(s.assignCell(0, 0, second));
    QVERIFY(!QFile::exists(s.clipPath(first.clipId)));
    QVERIFY(QFile::exists(s.clipPath(second.clipId)));
}

void SoundboardStoreTest::importEvictsOldestHistoryFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    SpeechHistory h(dir.path());
    QVERIFY(s.load());
    QVERIFY(h.load());
    s.setMaxTotalBytes(200);
    h.setMaxTotalBytes(200);
    const QString a = dir.filePath(QStringLiteral("a.mp3"));
    const QString b = dir.filePath(QStringLiteral("b.mp3"));
    QVERIFY(!writeBytes(a, 80).isEmpty());
    QVERIFY(!writeBytes(b, 80).isEmpty());
    QVERIFY(h.record(QStringLiteral("old"), QStringLiteral("eleven"), {}, {}, a));
    QVERIFY(h.record(QStringLiteral("new"), QStringLiteral("eleven"), {}, {}, b));
    QCOMPARE(h.items().size(), 2);
    const QString oldId = h.items().last().id;
    const QString newId = h.items().first().id;
    QVERIFY(QFile::exists(h.clipPath(oldId)));
    QString err;
    const QString clipId = s.importClip(a, &err, &h);
    QVERIFY2(!clipId.isEmpty(), qPrintable(err));
    QVERIFY(!QFile::exists(h.clipPath(oldId)));
    QVERIFY(QFile::exists(h.clipPath(newId)));
    QVERIFY(QFile::exists(s.clipPath(clipId)));
}

void SoundboardStoreTest::removeButtonAndSetColor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.newTopic());
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    QVERIFY(!writeBytes(src, 64).isEmpty());
    QString err;
    SoundboardButton seed;
    seed.label = QStringLiteral("Hi");
    seed.sourceText = QStringLiteral("Hi");
    seed.clipId = s.importClip(src, &err);
    QVERIFY2(!seed.clipId.isEmpty(), qPrintable(err));
    QVERIFY(s.assignCell(0, 0, seed));
    const QString id = s.buttonAt(*s.activeTopic(), 0, 0)->id;
    QVERIFY(s.setButtonColor(id, QStringLiteral("#8AB4F8")));
    QCOMPARE(s.findButton(id)->color.toUpper(), QStringLiteral("#8AB4F8"));
    QVERIFY(!s.setButtonColor(id, QStringLiteral("nope")));
    QVERIFY(s.setButtonLabel(id, QStringLiteral("Hello")));
    QCOMPARE(s.findButton(id)->label, QStringLiteral("Hello"));
    QVERIFY(!s.setButtonLabel(id, QStringLiteral("  ")));
    QCOMPARE(s.findButton(id)->label, QStringLiteral("Hello"));
    QVERIFY(s.removeButton(id));
    QVERIFY(!s.findButton(id));
    QVERIFY(!QFile::exists(s.clipPath(seed.clipId)));
    QVERIFY(!s.removeButton(id));
}

void SoundboardStoreTest::removeAndStyleTopic()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SoundboardStore s(dir.path());
    QVERIFY(s.load());
    QVERIFY(s.newTopic());
    const QString id = s.activeTopicId();
    QVERIFY(s.setTopicColor(id, QStringLiteral("#81C995")));
    QCOMPARE(s.activeTopic()->color.toUpper(), QStringLiteral("#81C995"));
    QVERIFY(s.setTopicIcon(id, QStringLiteral("pets")));
    QCOMPARE(s.activeTopic()->icon, QStringLiteral("pets"));
    QVERIFY(s.renameTopic(id, QStringLiteral("Animals")));
    QCOMPARE(s.activeTopic()->name, QStringLiteral("Animals"));
    QVERIFY(!s.setTopicIcon(id, QStringLiteral("../x")));
    QVERIFY(s.removeTopic(id));
    QVERIFY(!s.findTopic(id));
    while (s.topics().size() > 1) {
        QVERIFY(s.removeTopic(s.activeTopicId()));
    }
    QVERIFY(!s.removeTopic(s.activeTopicId()));
}

QObject* createSoundboardStoreTest()
{
    return new SoundboardStoreTest;
}

#include "SoundboardStoreTest.moc"
