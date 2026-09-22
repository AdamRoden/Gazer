#include "input/InjectGate.h"

#include <atomic>

namespace gazer {
namespace InjectGate {

namespace {
std::atomic<bool> g_paused{false};
}

void setPaused(bool on)
{
    g_paused.store(on, std::memory_order_release);
}

bool paused()
{
    return g_paused.load(std::memory_order_acquire);
}

#ifdef _WIN32
bool send(INPUT* inputs, UINT count)
{
    if (paused() || !inputs || count == 0) {
        return false;
    }
    return SendInput(count, inputs, sizeof(INPUT)) == count;
}
#endif

} // namespace InjectGate
} // namespace gazer
