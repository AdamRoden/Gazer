#include "app/SettingsCatalog.h"

#include <QJsonObject>
#include <QSet>
#include <QtTest>

using namespace gazer;

class SettingsCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void entriesAreUniqueAndNamed();
    void roundTripKeepsAFullSnapshot();
    void resetRestoresChosenKeys();
    void importClassifiesEmptyAndNewer();
};

void SettingsCatalogTest::entriesAreUniqueAndNamed()
{
    QSet<QString> ids;
    const QStringList pages = {QStringLiteral("Dwell"), QStringLiteral("Pointer"),
                               QStringLiteral("Gaze"),  QStringLiteral("Head"),
                               QStringLiteral("Speak"), QStringLiteral("Theme"),
                               QStringLiteral("Setup")};
    const SettingsCatalogEntry* entries = settingsCatalogEntries();
    QVERIFY(settingsCatalogCount() > 20);
    for (int i = 0; i < settingsCatalogCount(); ++i) {
        const SettingsCatalogEntry& entry = entries[i];
        const QString id = QLatin1String(entry.id);
        QVERIFY(!id.isEmpty());
        QVERIFY(!ids.contains(id));
        ids.insert(id);
        QVERIFY(pages.contains(QLatin1String(entry.page)));
        QVERIFY(QLatin1String(entry.title).size() > 0);
        QVERIFY(QLatin1String(entry.description).size() > 0);
        const QStringList keys = settingsCatalogJsonKeys(entry);
        QVERIFY(!keys.isEmpty());
        QVERIFY(!settingValueText(AppSettings::defaults(), id).isEmpty());
    }
}

void SettingsCatalogTest::roundTripKeepsAFullSnapshot()
{
    AppSettings edited = AppSettings::defaults();
    edited.dwellGraceMs = 333;
    edited.speechStyle = QStringLiteral("soft");
    edited.lookToScroll.hubPx = 48;
    edited.headPoseEnabled = true;
    edited.speechVolume = 2.0;
    edited.startDocked = true;

    AppSettings loaded = AppSettings::defaults();
    QVERIFY(loaded.applyJsonObject(edited.toJsonObject()));
    QCOMPARE(loaded.toJsonObject(), edited.toJsonObject());
}

void SettingsCatalogTest::resetRestoresChosenKeys()
{
    AppSettings edited = AppSettings::defaults();
    edited.dwellGraceMs = 321;
    edited.scanGraceMs = 222;
    edited.lookToScroll.hubPx = 12;
    edited.lookToMouse.hubPx = 13;
    edited.headPoseEnabled = true;
    edited.headPoseOriginSet = true;
    edited.voiceStylesSet = true;
    edited.voiceStyleNames = {QStringLiteral("soft")};
    edited.railFavoritesSet = true;
    AppSettings::RailFavorite pin;
    pin.kind = QStringLiteral("action");
    pin.ref = QStringLiteral("speak");
    edited.railFavorites = {pin};
    edited.mouseMoveMagPick = false;
    edited.startDocked = true;
    edited.themeBrightness = 1;

    QVERIFY(edited.resetKeysToDefaults({QStringLiteral("dwellGraceMs")}));
    QCOMPARE(edited.dwellGraceMs, AppSettings::defaults().dwellGraceMs);
    QCOMPARE(edited.scanGraceMs, 222);

    QVERIFY(edited.resetKeysToDefaults({QStringLiteral("lookToMaps.scroll")}));
    QCOMPARE(edited.lookToScroll.hubPx, AppSettings::defaults().lookToScroll.hubPx);
    QCOMPARE(edited.lookToMouse.hubPx, 13);

    QStringList all;
    const SettingsCatalogEntry* entries = settingsCatalogEntries();
    for (int i = 0; i < settingsCatalogCount(); ++i) {
        all.append(settingsCatalogJsonKeys(entries[i]));
    }
    QVERIFY(edited.resetKeysToDefaults(all));
    QCOMPARE(edited.toJsonObject(), AppSettings::defaults().toJsonObject());
    QVERIFY(!edited.resetKeysToDefaults({}));
}

void SettingsCatalogTest::importClassifiesEmptyAndNewer()
{
    QCOMPARE(classifySettingsImport(QJsonObject()), SettingsImportKind::Empty);
    QJsonObject unknown;
    unknown.insert(QStringLiteral("notASetting"), 1);
    QCOMPARE(classifySettingsImport(unknown), SettingsImportKind::Empty);

    QJsonObject schemaOnly;
    schemaOnly.insert(QStringLiteral("schemaVersion"), 2);
    QCOMPARE(classifySettingsImport(schemaOnly), SettingsImportKind::Ok);

    QJsonObject oneKey;
    oneKey.insert(QStringLiteral("showSplash"), false);
    QCOMPARE(classifySettingsImport(oneKey), SettingsImportKind::Ok);

    QJsonObject nested;
    QJsonObject maps;
    maps.insert(QStringLiteral("scroll"), QJsonObject());
    nested.insert(QStringLiteral("lookToMaps"), maps);
    QCOMPARE(classifySettingsImport(nested), SettingsImportKind::Ok);

    QJsonObject newer;
    newer.insert(QStringLiteral("schemaVersion"), AppSettings::kSettingsSchemaVersion + 1);
    QCOMPARE(classifySettingsImport(newer), SettingsImportKind::Newer);
}

QObject* createSettingsCatalogTest()
{
    return new SettingsCatalogTest;
}

#include "SettingsCatalogTest.moc"
