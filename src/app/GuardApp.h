#pragma once

namespace gazer {

/// `Gazer.exe --guard`: heartbeat watchdog, hung-host kill/relaunch, crash-loop
/// rescue board, Pause hotkey. Same signed image as the host (UIAccess band).
int runGuard(int argc, char** argv);

/// Start a detached `--guard` if one is not already holding the mutex.
bool spawnGuardDetached();

} // namespace gazer
