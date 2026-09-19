#include "assist/SidecarHost.h"

#include "assist/AhkLauncher.h"
#include "assist/ChildProcess.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtGlobal>

namespace gazer {

namespace {

void appendIfFile(QStringList& out, const QString& path)
{
    const QString p = QDir::cleanPath(path);
    if (p.isEmpty() || out.contains(p, Qt::CaseInsensitive)) {
        return;
    }
    if (QFileInfo::exists(p) && QFileInfo(p).isFile()) {
        out.push_back(p);
    }
}

bool isPyLauncherName(const QString& path)
{
    const QString n = QFileInfo(path).fileName().toLower();
    return n == QLatin1String("py.exe") || n == QLatin1String("py");
}

QString normalizeAbs(const QString& path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

QString findPythonIn(const QStringList& candidates, bool* pyLauncher)
{
    QString launcher;
    for (const QString& raw : candidates) {
        const QString path = QDir::cleanPath(raw);
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            continue;
        }
        if (isPyLauncherName(path)) {
            if (launcher.isEmpty()) {
                launcher = path;
            }
            continue;
        }
        if (pyLauncher) {
            *pyLauncher = false;
        }
        return path;
    }
    if (!launcher.isEmpty()) {
        if (pyLauncher) {
            *pyLauncher = true;
        }
        return launcher;
    }
    if (pyLauncher) {
        *pyLauncher = false;
    }
    return {};
}

} // namespace

SidecarHost::SidecarHost(QObject* parent)
    : QObject(parent)
{
}

SidecarHost::~SidecarHost()
{
    stopAll();
}

QStringList SidecarHost::defaultAllowedRoots()
{
    QStringList roots;
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        roots.push_back(appData);
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        roots.push_back(QDir(appDir).filePath(QStringLiteral("resources")));
    }
    return roots;
}

bool SidecarHost::pathIsUnder(const QString& absFile, const QString& absRoot)
{
    if (absFile.isEmpty() || absRoot.isEmpty()) {
        return false;
    }
    const QString file = normalizeAbs(absFile);
    QString root = normalizeAbs(absRoot);
    while (root.endsWith(QLatin1Char('/'))) {
        root.chop(1);
    }
    if (file.compare(root, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return file.startsWith(root + QLatin1Char('/'), Qt::CaseInsensitive);
}

bool SidecarHost::resolveScriptPath(const QString& file, const QStringList& roots, QString* absOut,
                                    QString* error)
{
    const QString raw = file.trimmed();
    if (raw.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Run needs a file");
        }
        return false;
    }
    auto acceptIfAllowed = [&](const QString& candidate) -> bool {
        if (!QFileInfo::exists(candidate) || !QFileInfo(candidate).isFile()) {
            return false;
        }
        const QString canon = QFileInfo(candidate).canonicalFilePath();
        const QString live = canon.isEmpty() ? candidate : canon;
        for (const QString& root : roots) {
            if (root.isEmpty()) {
                continue;
            }
            QString rootCanon = QFileInfo(root).canonicalFilePath();
            if (rootCanon.isEmpty()) {
                rootCanon = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
            }
            if (pathIsUnder(live, rootCanon)) {
                if (absOut) {
                    *absOut = live;
                }
                return true;
            }
        }
        return false;
    };

    const QFileInfo fi(raw);
    if (fi.isAbsolute()) {
        const QString candidate = QDir::cleanPath(fi.absoluteFilePath());
        if (acceptIfAllowed(candidate)) {
            return true;
        }
        if (error) {
            *error = QFileInfo::exists(candidate)
                         ? QStringLiteral("Run file is outside allowed directories")
                         : QStringLiteral("Run file not found: %1").arg(raw);
        }
        return false;
    }
    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        const QString candidate =
            QDir::cleanPath(QFileInfo(QDir(root).filePath(raw)).absoluteFilePath());
        if (acceptIfAllowed(candidate)) {
            return true;
        }
    }
    if (error) {
        *error = QStringLiteral("Run file not found: %1").arg(raw);
    }
    return false;
}

bool SidecarHost::scriptExtensionOk(SidecarKind kind, const QString& path, QString* error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (kind == SidecarKind::Python) {
        if (ext == QLatin1String("py") || ext == QLatin1String("pyw")) {
            return true;
        }
        if (error) {
            *error = QStringLiteral("Python Run expects a .py file");
        }
        return false;
    }
    if (ext == QLatin1String("ahk")) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("AHK Run expects a .ahk file");
    }
    return false;
}

QString SidecarHost::resolvePython(bool* pyLauncher, QString* error)
{
    if (pyLauncher) {
        *pyLauncher = false;
    }
    if (m_overridePython.has_value()) {
        const QString path = m_overridePython->trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            if (error) {
                *error = QStringLiteral("Python is not installed");
            }
            return {};
        }
        if (pyLauncher) {
            *pyLauncher = isPyLauncherName(path);
        }
        return QDir::cleanPath(path);
    }

    const QString env = qEnvironmentVariable("GAZER_PYTHON").trimmed();
    if (!env.isEmpty()) {
        if (QFileInfo::exists(env) && QFileInfo(env).isFile()) {
            if (pyLauncher) {
                *pyLauncher = isPyLauncherName(env);
            }
            return QDir::cleanPath(env);
        }
        if (error) {
            *error = QStringLiteral("GAZER_PYTHON does not point to Python");
        }
        return {};
    }

    QStringList candidates;
    for (const char* name : {"python", "python3", "py"}) {
        const QString found = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!found.isEmpty()) {
            appendIfFile(candidates, found);
        }
    }
    const QString picked = findPythonIn(candidates, pyLauncher);
    if (picked.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Python is not installed");
        }
        return {};
    }
    return picked;
}

void SidecarHost::setOverridePython(const std::optional<QString>& path)
{
    m_overridePython = path;
}

QString SidecarHost::resolveAhk(const QString& scriptPath, QString* error)
{
    if (!m_ahk) {
        if (error) {
            *error = QStringLiteral("AutoHotkey is not installed");
        }
        return {};
    }
    QString src;
    QFile f(scriptPath);
    if (f.open(QIODevice::ReadOnly)) {
        src = QString::fromUtf8(f.read(4096));
    }
    return m_ahk->findExecutable(src, error);
}

void SidecarHost::forgetProcess(QProcess* proc)
{
    if (!proc) {
        return;
    }
    for (auto it = m_resident.begin(); it != m_resident.end();) {
        if (it.value().proc == proc) {
            it = m_resident.erase(it);
        } else {
            ++it;
        }
    }
}

void SidecarHost::stopAll()
{
    ChildProcess::killChildren(this);
    m_resident.clear();
}

bool SidecarHost::run(const SidecarRequest& req, const QStringList& extraRoots, QString* error)
{
    QStringList roots;
    for (const QString& r : extraRoots) {
        if (!r.trimmed().isEmpty()) {
            roots.push_back(r);
        }
    }
    roots.append(defaultAllowedRoots());

    QString abs;
    if (!resolveScriptPath(req.file, roots, &abs, error)) {
        return false;
    }
    if (!scriptExtensionOk(req.kind, abs, error)) {
        return false;
    }

    const QString ident =
        !req.key.trimmed().isEmpty() ? req.key.trimmed() : (req.persist ? abs : QString());

    if (req.persist && !ident.isEmpty()) {
        auto it = m_resident.find(ident);
        if (it != m_resident.end()) {
            QProcess* p = it->proc;
            if (m_dryRun || (p && p->state() != QProcess::NotRunning)) {
                m_last = it->launch;
                return true;
            }
            m_resident.erase(it);
        }
    }

    bool pyLauncher = false;
    QString program;
    QStringList arguments;
    if (req.kind == SidecarKind::Python) {
        program = resolvePython(&pyLauncher, error);
        if (program.isEmpty()) {
            return false;
        }
        if (pyLauncher) {
            arguments << QStringLiteral("-3");
        }
        arguments << abs;
    } else {
        program = resolveAhk(abs, error);
        if (program.isEmpty()) {
            return false;
        }
        arguments << abs;
    }
    if (!req.args.trimmed().isEmpty()) {
        arguments.append(QProcess::splitCommand(req.args));
    }

    SidecarLaunch launch;
    launch.program = program;
    launch.arguments = arguments;
    launch.workDir = QFileInfo(abs).absolutePath();
    launch.key = ident;
    m_last = launch;

    if (m_dryRun) {
        if (req.persist && !ident.isEmpty()) {
            Resident r;
            r.launch = launch;
            m_resident.insert(ident, r);
        }
        return true;
    }

    const QHash<QString, QString> extraEnv{
        {QStringLiteral("GAZER_EXE"), QCoreApplication::applicationFilePath()},
        {QStringLiteral("GAZER_ACTION_PIPE"), m_actionPipe},
    };
    QProcess* proc = ChildProcess::start(this, program, arguments, launch.workDir, extraEnv,
                                         QStringLiteral("Run"),
                                         [this](QProcess* p) { forgetProcess(p); }, error);
    if (!proc) {
        return false;
    }
    if (req.persist && !ident.isEmpty()) {
        Resident r;
        r.proc = proc;
        r.launch = launch;
        m_resident.insert(ident, r);
    }
    return true;
}

} // namespace gazer
