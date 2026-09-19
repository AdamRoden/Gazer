#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <functional>

class QObject;
class QProcess;

namespace gazer {
namespace ChildProcess {

using DoneFn = std::function<void(QProcess*)>;

/// Start `program` as a child of `parent`. Does not wait for exit.
/// `onDone` runs once on exit, FailedToStart, or waitForStarted failure.
/// The process is `deleteLater`'d after `onDone`.
[[nodiscard]] QProcess* start(QObject* parent, const QString& program, const QStringList& arguments,
                              const QString& workDir, const QHash<QString, QString>& extraEnv,
                              const QString& logLabel, DoneFn onDone, QString* error);

/// Disconnect then kill. Safe from a parent destructor (`finished` must not
/// `deleteLater` children `QObject` is about to destroy).
void killChildren(QObject* parent);

} // namespace ChildProcess
} // namespace gazer
