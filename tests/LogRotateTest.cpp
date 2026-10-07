#include "utils/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

class LogRotateTest final : public QObject {
    Q_OBJECT

private slots:
    void missingAndEmptyStayPut();
    void copiesPreviousSessionAndKeepsNewest();
    void guardArchivesDoNotReplaceHost();
};

namespace {

QStringList archives(const QString& dir, const QString& pattern)
{
    return QDir(dir).entryList({pattern}, QDir::Files, QDir::Time);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

bool writeFile(const QString& path, const QByteArray& body)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return f.write(body) == body.size();
}

} // namespace

void LogRotateTest::missingAndEmptyStayPut()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString live = QDir(dir.path()).filePath(QStringLiteral("gazer.log"));
    QVERIFY(archiveSessionLog(live, dir.path(), 5));
    QCOMPARE(archives(dir.path(), QStringLiteral("gazer-*.log")).size(), 0);

    QVERIFY(writeFile(live, {}));
    QVERIFY(archiveSessionLog(live, dir.path(), 5));
    QCOMPARE(archives(dir.path(), QStringLiteral("gazer-*.log")).size(), 0);
    QCOMPARE(QFileInfo(live).size(), 0);
}

void LogRotateTest::copiesPreviousSessionAndKeepsNewest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString liveDir = QDir(dir.path()).filePath(QStringLiteral("live"));
    QVERIFY(QDir().mkpath(liveDir));
    const QString live = QDir(liveDir).filePath(QStringLiteral("gazer.log"));
    const QString logs = QDir(dir.path()).filePath(QStringLiteral("logs"));
    QVERIFY(writeFile(live, "old-session"));
    QVERIFY(archiveSessionLog(live, logs, 1));
    QCOMPARE(readFile(live), QByteArray("old-session"));

    const QStringList first = archives(logs, QStringLiteral("gazer-*.log"));
    QCOMPARE(first.size(), 1);
    const QString oldArchive = QDir(logs).filePath(first.first());
    QCOMPARE(readFile(oldArchive), QByteArray("old-session"));
    QFile aged(oldArchive);
    QVERIFY(aged.open(QIODevice::ReadWrite));
    QVERIFY(aged.setFileTime(QDateTime::currentDateTime().addDays(-2),
                             QFileDevice::FileModificationTime));
    aged.close();

    QVERIFY(writeFile(live, "new-session"));
    QVERIFY(archiveSessionLog(live, logs, 1));
    const QStringList left = archives(logs, QStringLiteral("gazer-*.log"));
    QCOMPARE(left.size(), 1);
    QCOMPARE(readFile(QDir(logs).filePath(left.first())), QByteArray("new-session"));
    QCOMPARE(readFile(live), QByteArray("new-session"));
}

void LogRotateTest::guardArchivesDoNotReplaceHost()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString logs = dir.path();
    const QString hostArchive = QDir(logs).filePath(QStringLiteral("gazer-20000101-000000-000.log"));
    QVERIFY(writeFile(hostArchive, "host"));
    const QString guard = QDir(logs).filePath(QStringLiteral("guard.log"));
    QVERIFY(writeFile(guard, "guard-session"));
    QVERIFY(archiveSessionLog(guard, logs, 1));
    QCOMPARE(readFile(hostArchive), QByteArray("host"));
    const QStringList guardArchives = archives(logs, QStringLiteral("guard-*.log"));
    QCOMPARE(guardArchives.size(), 1);
    QCOMPARE(readFile(QDir(logs).filePath(guardArchives.first())),
             QByteArray("guard-session"));
}

QObject* createLogRotateTest()
{
    return new LogRotateTest;
}

#include "LogRotateTest.moc"
