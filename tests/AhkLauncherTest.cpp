#include "assist/AhkLauncher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

namespace {

bool touch(const QString& path)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write("x");
    return true;
}

QString stripBom(QByteArray data)
{
    if (data.startsWith("\xEF\xBB\xBF")) {
        data = data.mid(3);
    }
    return QString::fromUtf8(data);
}

} // namespace

class AhkLauncherTest final : public QObject {
    Q_OBJECT

private slots:
    void ranksV2OverUx();
    void requiresV1PicksV1();
    void emptySourceIsNoop();
    void missingInstallFails();
    void dryRunWritesScript();
};

void AhkLauncherTest::ranksV2OverUx()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString v2 = tmp.filePath(QStringLiteral("v2/AutoHotkey64.exe"));
    const QString ux = tmp.filePath(QStringLiteral("AutoHotkey.exe"));
    const QString v1 = tmp.filePath(QStringLiteral("v1.1/AutoHotkeyU64.exe"));
    QVERIFY(touch(v2));
    QVERIFY(touch(ux));
    QVERIFY(touch(v1));

    const QString picked = AhkLauncher::findExecutableIn({ux, v1, v2}, QStringLiteral("Send \"a\""));
    QCOMPARE(QDir::fromNativeSeparators(picked), QDir::fromNativeSeparators(v2));
}

void AhkLauncherTest::requiresV1PicksV1()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString v2 = tmp.filePath(QStringLiteral("v2/AutoHotkey64.exe"));
    const QString ux = tmp.filePath(QStringLiteral("AutoHotkey.exe"));
    const QString v1 = tmp.filePath(QStringLiteral("v1.1/AutoHotkeyU64.exe"));
    QVERIFY(touch(v2));
    QVERIFY(touch(ux));
    QVERIFY(touch(v1));

    const QString src = QStringLiteral("#Requires AutoHotkey v1\nSend, a");
    const QString picked = AhkLauncher::findExecutableIn({v2, ux, v1}, src);
    QCOMPARE(QDir::fromNativeSeparators(picked), QDir::fromNativeSeparators(v1));
}

void AhkLauncherTest::emptySourceIsNoop()
{
    AhkLauncher ahk;
    ahk.setOverrideExecutable(QString());
    QString err;
    QVERIFY(ahk.run(QStringLiteral("  \n"), &err));
    QVERIFY(err.isEmpty());
}

void AhkLauncherTest::missingInstallFails()
{
    AhkLauncher ahk;
    ahk.setOverrideExecutable(QString());
    QString err;
    QVERIFY(!ahk.run(QStringLiteral("Send \"{Tab}\""), &err));
    QVERIFY(err.contains(QStringLiteral("not installed")));
}

void AhkLauncherTest::dryRunWritesScript()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString fake = tmp.filePath(QStringLiteral("AutoHotkey64.exe"));
    QVERIFY(touch(fake));

    AhkLauncher ahk;
    ahk.setDryRun(true);
    ahk.setOverrideExecutable(fake);
    const QString src = QStringLiteral("{\n    Send \"{LAlt down}{Tab}{LAlt up}\"\n}");
    QString err;
    QVERIFY2(ahk.run(src, &err), qPrintable(err));
    QVERIFY(!ahk.lastScriptPath().isEmpty());
    QFile f(ahk.lastScriptPath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(stripBom(f.readAll()).trimmed(), src);
    f.close();
    QVERIFY(QFile::remove(ahk.lastScriptPath()));
}

QObject* createAhkLauncherTest()
{
    return new AhkLauncherTest;
}

#include "AhkLauncherTest.moc"
