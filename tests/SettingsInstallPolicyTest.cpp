#include "app/SettingsInstallPolicy.h"

#include <QFile>
#include <QtTest>

using namespace gazer;

class SettingsInstallPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void everyInstallKindPreservesUnlessReset();
    void installerRemovesSettingsOnlyWhenResetRequested();
};

void SettingsInstallPolicyTest::everyInstallKindPreservesUnlessReset()
{
    const SettingsInstallKind kinds[] = {
        SettingsInstallKind::Fresh,
        SettingsInstallKind::Upgrade,
        SettingsInstallKind::Repair,
        SettingsInstallKind::SameVersionReinstall,
    };
    for (const SettingsInstallKind kind : kinds) {
        const SettingsInstallDecision keep = decideSettingsInstall(kind, false);
        QVERIFY(keep.preserveSettings);
        QVERIFY(!keep.factoryReset);

        const SettingsInstallDecision reset = decideSettingsInstall(kind, true);
        QVERIFY(!reset.preserveSettings);
        QVERIFY(reset.factoryReset);
    }
}

void SettingsInstallPolicyTest::installerRemovesSettingsOnlyWhenResetRequested()
{
    QFile wxs(QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/packaging/Gazer.wxs"));
    QVERIFY2(wxs.open(QIODevice::ReadOnly), qPrintable(wxs.errorString()));
    const QString text = QString::fromUtf8(wxs.readAll());

    QVERIFY(text.contains(QStringLiteral("RESETUSERSETTINGS")));
    QVERIFY(text.contains(QStringLiteral("Id=\"FactoryResetSettings\"")));
    QVERIFY(text.contains(QStringLiteral("Condition=\"RESETUSERSETTINGS = &quot;1&quot;\"")));

    const int preserveAt = text.indexOf(QStringLiteral("Id=\"ResetUserSettings\""));
    const int factoryAt = text.indexOf(QStringLiteral("Id=\"FactoryResetSettings\""));
    const int removeAt = text.indexOf(QStringLiteral("Id=\"RemoveSettingsJson\""));
    QVERIFY(preserveAt > 0);
    QVERIFY(factoryAt > preserveAt);
    QVERIFY(removeAt > factoryAt);

    const int preserveEnd = text.indexOf(QStringLiteral("<Component"), preserveAt + 1);
    QVERIFY(preserveEnd > preserveAt);
    const QString preserveBody = text.mid(preserveAt, preserveEnd - preserveAt);
    QVERIFY(!preserveBody.contains(QStringLiteral("RemoveFile")));
    QVERIFY(!preserveBody.contains(QStringLiteral("settings.json")));
    // Same GUID as the shipped component. The keypath name must stay
    // ResetSettings; renaming it under this GUID breaks major upgrade.
    QVERIFY(preserveBody.contains(QStringLiteral("D4E6A8B0-3F5C-7D9E-1B2A-4C6D8E0F1A3B")));
    QVERIFY(preserveBody.contains(QStringLiteral("Name=\"ResetSettings\"")));
    QVERIFY(!text.contains(QStringLiteral("SettingsPreserved")));
}

QObject* createSettingsInstallPolicyTest()
{
    return new SettingsInstallPolicyTest;
}

#include "SettingsInstallPolicyTest.moc"
