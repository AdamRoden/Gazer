#include "assist/Mp3File.h"
#include "assist/PcmWav.h"
#include "assist/SpeechHistory.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

class SpeechHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void missingFileIsEmpty();
    void recordPrependsAndCaps();
    void copiesMpegAndRejectsBadId();
    void recordTwoMpegFilesKeepBoth();
    void makeRoomEvictsOldest();
    void removeDropsRowAndMpeg();
    void saveMp3CopiesBytes();
    void saveWavEncodesMp3();
};

void SpeechHistoryTest::missingFileIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    QVERIFY(h.items().isEmpty());
}

void SpeechHistoryTest::recordPrependsAndCaps()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    QVERIFY(h.record(QStringLiteral("one"), QStringLiteral("sapi"), {}, {}, {}));
    QVERIFY(h.record(QStringLiteral("two"), QStringLiteral("sapi"), {}, {}, {}));
    QCOMPARE(h.items().size(), 2);
    QCOMPARE(h.items().first().phrase, QStringLiteral("two"));
    for (int i = 0; i < SpeechHistory::kMaxItems + 5; ++i) {
        QVERIFY(h.record(QStringLiteral("p%1").arg(i), QStringLiteral("sapi"), {}, {}, {}));
    }
    QCOMPARE(h.items().size(), SpeechHistory::kMaxItems);
    QCOMPARE(h.items().first().phrase, QStringLiteral("p%1").arg(SpeechHistory::kMaxItems + 4));
    SpeechHistory b(dir.path());
    QVERIFY(b.load());
    QCOMPARE(b.items().size(), SpeechHistory::kMaxItems);
}

void SpeechHistoryTest::copiesMpegAndRejectsBadId()
{
    QVERIFY(!SpeechHistory::validId(QStringLiteral("../x")));
    QVERIFY(SpeechHistory::validId(QStringLiteral("deadbeef")));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    {
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(32, 'M'));
    }
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    QString copied;
    QVERIFY(h.record(QStringLiteral("hi"), QStringLiteral("eleven"), QStringLiteral("eleven_v3"),
                     QStringLiteral("abc"), src, nullptr, &copied));
    QCOMPARE(h.items().size(), 1);
    QVERIFY(QFile::exists(h.clipPath(h.items().first().id)));
    QCOMPARE(copied, h.clipPath(h.items().first().id));
    QVERIFY(copied.endsWith(QStringLiteral(".mp3")));

    const QString wav = dir.filePath(QStringLiteral("src.wav"));
    {
        QFile f(wav);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(32, 'W'));
    }
    QString copiedWav;
    QVERIFY(h.record(QStringLiteral("wave"), QStringLiteral("eleven"),
                     QStringLiteral("eleven_v4_turbo"), QStringLiteral("abc"), wav, nullptr,
                     &copiedWav));
    QVERIFY(copiedWav.endsWith(QStringLiteral(".wav")));
    QCOMPARE(h.clipPath(h.items().first().id), copiedWav);
}

void SpeechHistoryTest::recordTwoMpegFilesKeepBoth()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src1 = dir.filePath(QStringLiteral("1.mp3"));
    const QString src2 = dir.filePath(QStringLiteral("2.mp3"));
    {
        QFile f(src1);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(32, 'A'));
    }
    {
        QFile f(src2);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(32, 'B'));
    }
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    QVERIFY(h.record(QStringLiteral("one"), QStringLiteral("eleven"), {}, {}, src1));
    QVERIFY(h.record(QStringLiteral("two"), QStringLiteral("eleven"), {}, {}, src2));
    QCOMPARE(h.items().size(), 2);
    QVERIFY(QFile::exists(h.clipPath(h.items()[0].id)));
    QVERIFY(QFile::exists(h.clipPath(h.items()[1].id)));
    QVERIFY(QFile::exists(src1));
    QVERIFY(QFile::exists(src2));
}

void SpeechHistoryTest::makeRoomEvictsOldest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    h.setMaxTotalBytes(200);
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    {
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(80, 'M'));
    }
    QVERIFY(h.record(QStringLiteral("old"), QStringLiteral("eleven"), {}, {}, src));
    QVERIFY(h.record(QStringLiteral("new"), QStringLiteral("eleven"), {}, {}, src));
    QCOMPARE(h.items().size(), 2);
    const QString oldId = h.items().last().id;
    h.makeRoom(80);
    QCOMPARE(h.items().size(), 1);
    QCOMPARE(h.items().first().phrase, QStringLiteral("new"));
    QVERIFY(!QFile::exists(h.clipPath(oldId)));
}

void SpeechHistoryTest::removeDropsRowAndMpeg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    {
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(32, 'M'));
    }
    SpeechHistory h(dir.path());
    QVERIFY(h.load());
    QVERIFY(h.record(QStringLiteral("keep"), QStringLiteral("eleven"), {}, {}, src));
    QVERIFY(h.record(QStringLiteral("drop"), QStringLiteral("eleven"), {}, {}, src));
    QCOMPARE(h.items().size(), 2);
    const QString dropId = h.items().first().id;
    const QString keepId = h.items().last().id;
    const QString dropPath = h.clipPath(dropId);
    QVERIFY(QFile::exists(dropPath));
    QVERIFY(!h.remove(QStringLiteral("../x")));
    QVERIFY(h.remove(dropId));
    QCOMPARE(h.items().size(), 1);
    QCOMPARE(h.items().first().id, keepId);
    QVERIFY(!QFile::exists(dropPath));
    QVERIFY(QFile::exists(h.clipPath(keepId)));
    SpeechHistory b(dir.path());
    QVERIFY(b.load());
    QCOMPARE(b.items().size(), 1);
    QCOMPARE(b.items().first().id, keepId);
}

void SpeechHistoryTest::saveMp3CopiesBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QByteArray bytes("ID3not-a-real-frame");
    const QString src = dir.filePath(QStringLiteral("src.mp3"));
    const QString dest = dir.filePath(QStringLiteral("out.mp3"));
    QFile in(src);
    QVERIFY(in.open(QIODevice::WriteOnly));
    QCOMPARE(in.write(bytes), qint64(bytes.size()));
    in.close();
    QString err;
    QVERIFY2(writeMp3File(src, dest, &err), qPrintable(err));
    QFile out(dest);
    QVERIFY(out.open(QIODevice::ReadOnly));
    QCOMPARE(out.readAll(), bytes);
}

void SpeechHistoryTest::saveWavEncodesMp3()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray pcm(24000, '\0');
    for (int i = 0; i < pcm.size() / 2; ++i) {
        const qint16 s = qint16(8000 * ((i / 20) % 2 == 0 ? 1 : -1));
        pcm[i * 2] = char(s & 0xFF);
        pcm[i * 2 + 1] = char((s >> 8) & 0xFF);
    }
    const QString src = dir.filePath(QStringLiteral("src.wav"));
    QVERIFY(PcmWav::writeFile(src, pcm, 24000));
    const QString dest = dir.filePath(QStringLiteral("out.mp3"));
    QString err;
    QVERIFY2(writeMp3File(src, dest, &err), qPrintable(err));
    QFile out(dest);
    QVERIFY(out.open(QIODevice::ReadOnly));
    const QByteArray saved = out.readAll();
    QVERIFY(saved.size() > 32);
    const bool id3 = saved.startsWith("ID3");
    const bool sync = saved.size() >= 2 && quint8(saved[0]) == 0xFF && (quint8(saved[1]) & 0xE0) == 0xE0;
    QVERIFY(id3 || sync);
}

QObject* createSpeechHistoryTest()
{
    return new SpeechHistoryTest;
}

#include "SpeechHistoryTest.moc"
