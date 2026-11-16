#include "app/SettingsInstallPolicy.h"

#include <QFile>
#include <QtTest>

using namespace gazer;

class SettingsInstallPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void everyInstallKindPreservesSettings();
    void installerDoesNotRemoveSettings();
};

void SettingsInstallPolicyTest::everyInstallKindPreservesSettings()
{
    const SettingsInstallKind kinds[] = {
        SettingsInstallKind::Fresh,
        SettingsInstallKind::Upgrade,
        SettingsInstallKind::Repair,
        SettingsInstallKind::SameVersionReinstall,
    };
    for (const SettingsInstallKind kind : kinds) {
        const SettingsInstallDecision keep = decideSettingsInstall(kind);
        QVERIFY(keep.preserveSettings);
        QVERIFY(!keep.factoryReset);
    }
}

void SettingsInstallPolicyTest::installerDoesNotRemoveSettings()
{
    QFile wxs(QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/packaging/Gazer.wxs"));
    QVERIFY2(wxs.open(QIODevice::ReadOnly), qPrintable(wxs.errorString()));
    const QString text = QString::fromUtf8(wxs.readAll());

    QVERIFY(!text.contains(QStringLiteral("RESETUSERSETTINGS")));
    QVERIFY(!text.contains(QStringLiteral("FactoryResetSettings")));
    QVERIFY(!text.contains(QStringLiteral("RemoveSettingsJson")));
    QVERIFY(!text.contains(QStringLiteral("Reset my settings")));

    const int preserveAt = text.indexOf(QStringLiteral("<Component Id=\"ResetUserSettings\""));
    QVERIFY(preserveAt > 0);
    const int preserveEnd = text.indexOf(QStringLiteral("<Component"), preserveAt + 1);
    QVERIFY(preserveEnd > preserveAt);
    const QString preserveBody = text.mid(preserveAt, preserveEnd - preserveAt);
    QVERIFY(!preserveBody.contains(QStringLiteral("RemoveFile")));
    QVERIFY(!preserveBody.contains(QStringLiteral("settings.json")));
    // Same GUID as the shipped component. The keypath name must stay
    // ResetSettings; renaming it under this GUID breaks major upgrade.
    QVERIFY(preserveBody.contains(QStringLiteral("D4E6A8B0-3F5C-7D9E-1B2A-4C6D8E0F1A3B")));
    QVERIFY(preserveBody.contains(QStringLiteral("Name=\"ResetSettings\"")));
    QVERIFY(text.contains(QStringLiteral("RegisterGazerUpdateTask")));
    QVERIFY(text.contains(QStringLiteral("RemoveGazerUpdateTask")));
}

QObject* createSettingsInstallPolicyTest()
{
    return new SettingsInstallPolicyTest;
}

#include "SettingsInstallPolicyTest.moc"
