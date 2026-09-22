#pragma once

#include <QString>
#include <QtGlobal>

namespace gazer {
namespace WinProcess {

[[nodiscard]] QString imageBase(quint32 pid);
[[nodiscard]] QString selfImageBase();
/// True when the image basename is `Gazer.exe`.
[[nodiscard]] bool isGazerImage(quint32 pid);
[[nodiscard]] bool alive(quint32 pid);
/// Dump then `TerminateProcess`. Only `Gazer.exe`, never this pid.
[[nodiscard]] bool terminate(quint32 pid, QString* error = nullptr);

} // namespace WinProcess
} // namespace gazer
