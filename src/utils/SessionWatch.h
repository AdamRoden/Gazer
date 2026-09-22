#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

class QWindow;

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

/// Lock screen / Secure Desktop / session switch. Pauses OS inject while the
/// user cannot see Gazer.
class SessionWatch final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit SessionWatch(QObject* parent = nullptr);
    ~SessionWatch() override;

    [[nodiscard]] bool injectPaused() const { return m_paused; }

public slots:
    void setPaused(bool on);

signals:
    void injectPausedChanged(bool paused);

private:
#ifdef Q_OS_WIN
    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;
    static void CALLBACK desktopHook(HWINEVENTHOOK, DWORD event, HWND, LONG, LONG, DWORD,
                                     DWORD);
#endif

#ifdef Q_OS_WIN
    QWindow* m_probe = nullptr;
    HWND m_hwnd = nullptr;
    HWINEVENTHOOK m_hook = nullptr;
    static SessionWatch* s_self;
#endif
    bool m_paused = false;
};

} // namespace gazer
