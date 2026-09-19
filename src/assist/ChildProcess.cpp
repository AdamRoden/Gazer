#include "assist/ChildProcess.h"

#include "utils/Log.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <memory>

namespace gazer {
namespace ChildProcess {

QProcess* start(QObject* parent, const QString& program, const QStringList& arguments,
                const QString& workDir, const QHash<QString, QString>& extraEnv,
                const QString& logLabel, DoneFn onDone, QString* error)
{
    auto* proc = new QProcess(parent);
    proc->setProgram(program);
    proc->setArguments(arguments);
    proc->setWorkingDirectory(workDir);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    if (!extraEnv.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        for (auto it = extraEnv.constBegin(); it != extraEnv.constEnd(); ++it) {
            env.insert(it.key(), it.value());
        }
        proc->setProcessEnvironment(env);
    }

    auto state = std::make_shared<bool>(false);
    auto finishOnce = [state, proc, onDone = std::move(onDone)]() {
        if (*state) {
            return;
        }
        *state = true;
        if (onDone) {
            onDone(proc);
        }
        proc->deleteLater();
    };

    QObject::connect(proc, &QProcess::finished, parent,
                     [proc, logLabel, finishOnce](int code, QProcess::ExitStatus) {
                         if (code != 0) {
                             const QByteArray err = proc->readAll();
                             GAZER_WARN << logLabel << "exited" << code
                                        << QString::fromLocal8Bit(err);
                         }
                         finishOnce();
                     });
    QObject::connect(proc, &QProcess::errorOccurred, parent,
                     [proc, logLabel, finishOnce](QProcess::ProcessError e) {
                         if (e == QProcess::FailedToStart) {
                             GAZER_WARN << logLabel << "failed to start" << proc->errorString();
                             finishOnce();
                         }
                     });

    proc->start();
    if (!proc->waitForStarted(3000)) {
        if (error) {
            *error = proc->errorString().isEmpty()
                         ? QStringLiteral("%1 failed to start").arg(logLabel)
                         : proc->errorString();
        }
        finishOnce();
        return nullptr;
    }
    return proc;
}

void killChildren(QObject* parent)
{
    if (!parent) {
        return;
    }
    const auto procs = parent->findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect();
        p->kill();
        p->waitForFinished(500);
    }
}

} // namespace ChildProcess
} // namespace gazer
