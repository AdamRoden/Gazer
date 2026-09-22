#pragma once

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

/// Process-wide inject pause (lock screen / Secure Desktop / session switch).
namespace InjectGate {

void setPaused(bool on);
[[nodiscard]] bool paused();

#ifdef _WIN32
/// `SendInput` unless paused.
[[nodiscard]] bool send(INPUT* inputs, UINT count);
#endif

} // namespace InjectGate
} // namespace gazer
