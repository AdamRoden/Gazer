#include "assist/AhkLauncher.h"
#include "assist/SidecarHost.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

namespace {

bool writeText(const QString& path, const QString& body)
{
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir)) {
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(body.toUtf8());
    return true;
}

} // namespace

class SidecarHostTest final : public QObject {
    Q_OBJECT

private slots:
    void pathIsUnder();
    void resolveRelativeAndJail();
    void rejectWrongExtension();
    void missingPythonFails();
    void dryRunPythonRecordsArgs();
    void persistSecondStartIsNoop();
    void dryRunAhkUsesLauncher();
};

void SidecarHostTest::pathIsUnder()
{
    QVERIFY(SidecarHost::pathIsUnder(QStringLiteral("C:/Gazer/layouts/a.py"),
                                     QStringLiteral("C:/Gazer")));
    QVERIFY(SidecarHost::pathIsUnder(QStringLiteral("C:/Gazer"), QStringLiteral("C:/Gazer")));
    QVERIFY(!SidecarHost::pathIsUnder(QStringLiteral("C:/Windows/notepad.exe"),
                                      QStringLiteral("C:/Gazer")));
    QVERIFY(!SidecarHost::pathIsUnder(QStringLiteral("C:/Gazer2/x.py"), QStringLiteral("C:/Gazer")));
}

void SidecarHostTest::resolveRelativeAndJail()
{
    QTemporaryDir allowed;
    QTemporaryDir other;
    QVERIFY(allowed.isValid());
    QVERIFY(other.isValid());
    const QString ok = allowed.filePath(QStringLiteral("scripts/ok.py"));
    const QString evil = other.filePath(QStringLiteral("evil.py"));
    QVERIFY(writeText(ok, QStringLiteral("print(1)\n")));
    QVERIFY(writeText(evil, QStringLiteral("print(2)\n")));

    QString abs;
    QString err;
    QVERIFY2(SidecarHost::resolveScriptPath(QStringLiteral("scripts/ok.py"), {allowed.path()},
                                            &abs, &err),
             qPrintable(err));
    QCOMPARE(QDir::fromNativeSeparators(abs).toLower(),
             QDir::fromNativeSeparators(QFileInfo(ok).canonicalFilePath()).toLower());

    QVERIFY(!SidecarHost::resolveScriptPath(evil, {allowed.path()}, &abs, &err));
    QVERIFY(err.contains(QStringLiteral("outside")));

    QVERIFY(!SidecarHost::resolveScriptPath(QStringLiteral("missing.py"), {allowed.path()}, &abs,
                                            &err));
    QVERIFY(err.contains(QStringLiteral("not found")));
}

void SidecarHostTest::rejectWrongExtension()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString txt = tmp.filePath(QStringLiteral("note.txt"));
    QVERIFY(writeText(txt, QStringLiteral("hi\n")));
    SidecarHost host;
    host.setDryRun(true);
    host.setOverridePython(txt);
    SidecarRequest req;
    req.kind = SidecarKind::Python;
    req.file = txt;
    QString err;
    QVERIFY(!host.run(req, {tmp.path()}, &err));
    QVERIFY(err.contains(QStringLiteral(".py")));
}

void SidecarHostTest::missingPythonFails()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString py = tmp.filePath(QStringLiteral("t.py"));
    QVERIFY(writeText(py, QStringLiteral("print(1)\n")));
    SidecarHost host;
    host.setDryRun(true);
    host.setOverridePython(QString());
    SidecarRequest req;
    req.kind = SidecarKind::Python;
    req.file = py;
    QString err;
    QVERIFY(!host.run(req, {tmp.path()}, &err));
    QVERIFY(err.contains(QStringLiteral("not installed")));
}

void SidecarHostTest::dryRunPythonRecordsArgs()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString py = tmp.filePath(QStringLiteral("t.py"));
    const QString python = tmp.filePath(QStringLiteral("python.exe"));
    QVERIFY(writeText(py, QStringLiteral("print(1)\n")));
    QVERIFY(writeText(python, QStringLiteral("x")));
    SidecarHost host;
    host.setDryRun(true);
    host.setOverridePython(python);
    SidecarRequest req;
    req.kind = SidecarKind::Python;
    req.file = QStringLiteral("t.py");
    req.args = QStringLiteral("--pipe Gazer");
    QString err;
    QVERIFY2(host.run(req, {tmp.path()}, &err), qPrintable(err));
    const SidecarLaunch launch = host.lastLaunch();
    QCOMPARE(QDir::fromNativeSeparators(launch.program), QDir::fromNativeSeparators(python));
    QVERIFY(launch.arguments.contains(QFileInfo(py).canonicalFilePath())
            || launch.arguments.contains(QDir::fromNativeSeparators(py)));
    QCOMPARE(launch.arguments.last(), QStringLiteral("Gazer"));
    QVERIFY(launch.arguments.contains(QStringLiteral("--pipe")));
    QCOMPARE(QDir::fromNativeSeparators(launch.workDir), QDir::fromNativeSeparators(tmp.path()));
}

void SidecarHostTest::persistSecondStartIsNoop()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString py = tmp.filePath(QStringLiteral("t.py"));
    const QString python = tmp.filePath(QStringLiteral("python.exe"));
    QVERIFY(writeText(py, QStringLiteral("print(1)\n")));
    QVERIFY(writeText(python, QStringLiteral("x")));
    SidecarHost host;
    host.setDryRun(true);
    host.setOverridePython(python);
    SidecarRequest req;
    req.kind = SidecarKind::Python;
    req.file = py;
    req.persist = true;
    req.key = QStringLiteral("pred");
    QString err;
    QVERIFY2(host.run(req, {tmp.path()}, &err), qPrintable(err));
    QCOMPARE(host.lastLaunch().key, QStringLiteral("pred"));
    req.args = QStringLiteral("--ignored");
    QVERIFY2(host.run(req, {tmp.path()}, &err), qPrintable(err));
    QVERIFY(!host.lastLaunch().arguments.contains(QStringLiteral("--ignored")));
}

void SidecarHostTest::dryRunAhkUsesLauncher()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString script = tmp.filePath(QStringLiteral("snap.ahk"));
    const QString ahk = tmp.filePath(QStringLiteral("v2/AutoHotkey64.exe"));
    QVERIFY(writeText(script, QStringLiteral("Send \"a\"\n")));
    QVERIFY(writeText(ahk, QStringLiteral("x")));
    AhkLauncher launcher;
    launcher.setOverrideExecutable(ahk);
    SidecarHost host;
    host.setAhk(&launcher);
    host.setDryRun(true);
    SidecarRequest req;
    req.kind = SidecarKind::Ahk;
    req.file = QStringLiteral("snap.ahk");
    QString err;
    QVERIFY2(host.run(req, {tmp.path()}, &err), qPrintable(err));
    const SidecarLaunch launch = host.lastLaunch();
    QCOMPARE(QDir::fromNativeSeparators(launch.program), QDir::fromNativeSeparators(ahk));
    QVERIFY(launch.arguments.contains(QFileInfo(script).canonicalFilePath())
            || QDir::fromNativeSeparators(launch.arguments.value(0))
                   .endsWith(QStringLiteral("snap.ahk")));
}

QObject* createSidecarHostTest()
{
    return new SidecarHostTest;
}

#include "SidecarHostTest.moc"
