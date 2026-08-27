#include "utils/WinOverlay.h"

namespace gazer {

namespace {
OverlayStackWatch* g_watch = nullptr;
}

OverlayStackWatch::OverlayStackWatch(std::function<void()> restack, QObject* parent)
    : QObject(parent)
    , m_restack(std::move(restack))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    QObject::connect(&m_debounce, &QTimer::timeout, this, [this]() {
        if (m_restack) {
            m_restack();
        }
    });
    g_watch = this;
#ifdef Q_OS_WIN
    m_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                             &OverlayStackWatch::hookProc, 0, 0,
                             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
#endif
}

OverlayStackWatch::~OverlayStackWatch()
{
#ifdef Q_OS_WIN
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
#endif
    if (g_watch == this) {
        g_watch = nullptr;
    }
}

#ifdef Q_OS_WIN
void CALLBACK OverlayStackWatch::hookProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD)
{
    if (g_watch) {
        g_watch->m_debounce.start();
    }
}
#endif

} // namespace gazer
