#include "app/AppSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

namespace {

bool writeText(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return f.write(bytes) == bytes.size();
}

QByteArray readText(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

} // namespace

class SettingsMigrationTest final : public QObject {
    Q_OBJECT

private slots:
    void upgradePreservesDwellAndWritesBackup();
    void sameVersionReloadDoesNotRewrite();
    void malformedFileFallsBackAndKeepsCopy();
    void restoreFromBackupAfterCorruptFile();
    void explicitRestoreRollsBackMigration();
    void legacyNestedSettingsAreAdopted();
    void newerSchemaIsNotRewritten();
};

void SettingsMigrationTest::upgradePreservesDwellAndWritesBackup()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeText(path, R"({"schemaVersion":1,"dwellSequence":[321,654]})"));

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::Migrated);
    QCOMPARE(loaded.dwellSequence, QVector<int>({321, 654}));
    QVERIFY(QFile::exists(path + QStringLiteral(".bak")));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("settings.v1.json.bak"))));

    const QJsonObject rewritten = QJsonDocument::fromJson(readText(path)).object();
    QCOMPARE(rewritten.value(QStringLiteral("schemaVersion")).toInt(), 2);
    AppSettings again;
    QVERIFY(again.loadFromFile(path));
    QCOMPARE(again.dwellSequence, QVector<int>({321, 654}));
}

void SettingsMigrationTest::sameVersionReloadDoesNotRewrite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    AppSettings original;
    original.dwellSequence = {321, 654};
    original.dwellGraceMs = 240;
    QVERIFY(original.saveToFile(path));
    const QByteArray before = readText(path);

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::Loaded);
    QCOMPARE(loaded.dwellSequence, QVector<int>({321, 654}));
    QCOMPARE(loaded.dwellGraceMs, 240);
    QCOMPARE(readText(path), before);
    QVERIFY(!QFile::exists(path + QStringLiteral(".bak")));
}

void SettingsMigrationTest::malformedFileFallsBackAndKeepsCopy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeText(path, "{ this is not json"));

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::FactoryFallback);
    QCOMPARE(loaded.dwellSequence, AppSettings::defaultDwellSequence());

    const QStringList bad = QDir(dir.path()).entryList({QStringLiteral("settings.bad-*.json")});
    QCOMPARE(bad.size(), 1);
    QVERIFY(readText(dir.filePath(bad.first())).contains("not json"));

    AppSettings fromDisk;
    QVERIFY(fromDisk.loadFromFile(path));
    QCOMPARE(fromDisk.dwellSequence, AppSettings::defaultDwellSequence());
}

void SettingsMigrationTest::restoreFromBackupAfterCorruptFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    AppSettings original;
    original.dwellSequence = {222, 333};
    QVERIFY(original.saveToFile(path));
    QVERIFY(AppSettings::backupExistingFile(path));
    QVERIFY(writeText(path, "[]"));

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::RecoveredFromBackup);
    QCOMPARE(loaded.dwellSequence, QVector<int>({222, 333}));
    QVERIFY(!report.backupPath.isEmpty());
}

void SettingsMigrationTest::explicitRestoreRollsBackMigration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(writeText(path, R"({"schemaVersion":1,"dwellSequence":[515]})"));

    AppSettings migrated;
    const AppSettings::SettingsLoadReport report = migrated.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::Migrated);
    QVERIFY(AppSettings::restoreFromBackup(path));

    const QByteArray rolled = readText(path);
    QVERIFY(rolled.contains("\"schemaVersion\":1") || rolled.contains("\"schemaVersion\": 1"));
    AppSettings restored;
    QVERIFY(restored.loadFromFile(path));
    QCOMPARE(restored.dwellSequence, QVector<int>({515}));
}

void SettingsMigrationTest::legacyNestedSettingsAreAdopted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    const QString legacy = dir.filePath(QStringLiteral("Gazer/settings.json"));
    QVERIFY(writeText(legacy, R"({"schemaVersion":2,"dwellSequence":[111,222]})"));

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::Migrated);
    QCOMPARE(loaded.dwellSequence, QVector<int>({111, 222}));
    QVERIFY(QFile::exists(path));
    QVERIFY(QFile::exists(legacy));
}

void SettingsMigrationTest::newerSchemaIsNotRewritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    const QByteArray original = R"({"schemaVersion":99,"dwellSequence":[410],"futureKnob":true})";
    QVERIFY(writeText(path, original));

    AppSettings loaded;
    const AppSettings::SettingsLoadReport report = loaded.loadManaged(path);
    QCOMPARE(report.status, AppSettings::SettingsLoadReport::Status::KeptNewer);
    QCOMPARE(report.schemaVersion, 99);
    QCOMPARE(loaded.dwellSequence, QVector<int>({410}));
    QCOMPARE(readText(path), original);
}

QObject* createSettingsMigrationTest()
{
    return new SettingsMigrationTest;
}

#include "SettingsMigrationTest.moc"
