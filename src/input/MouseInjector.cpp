#include "input/MouseInjector.h"

#include "utils/Log.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

namespace {

#ifdef Q_OS_WIN

bool buttonFlags(const QString& button, DWORD* down, DWORD* up, QString* error)
{
    const QString b = button.toLower();
    if (b == QLatin1String("left") || b.isEmpty()) {
        *down = MOUSEEVENTF_LEFTDOWN;
        *up = MOUSEEVENTF_LEFTUP;
        return true;
    }
    if (b == QLatin1String("right")) {
        *down = MOUSEEVENTF_RIGHTDOWN;
        *up = MOUSEEVENTF_RIGHTUP;
        return true;
    }
    if (b == QLatin1String("middle")) {
        *down = MOUSEEVENTF_MIDDLEDOWN;
        *up = MOUSEEVENTF_MIDDLEUP;
        return true;
    }
    if (error) {
        *error = QStringLiteral("Unknown mouse button: %1").arg(button);
    }
    return false;
}

bool sendMouseFlag(DWORD flag, DWORD data = 0)
{
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    in.mi.mouseData = data;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

#endif

} // namespace

bool MouseInjector::click(const QString& button, QString* error)
{
#ifdef Q_OS_WIN
    DWORD down = 0;
    DWORD up = 0;
    if (!buttonFlags(button, &down, &up, error)) {
        return false;
    }

    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = down;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = up;
    if (SendInput(2, inputs, sizeof(INPUT)) != 2) {
        if (error) {
            *error = QStringLiteral("SendInput mouse click failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(button);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::doubleClick(const QString& button, QString* error)
{
#ifdef Q_OS_WIN
    // Two full clicks; system double-click timing accepts rapid pair.
    if (!click(button, error)) {
        return false;
    }
    return click(button, error);
#else
    Q_UNUSED(button);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::buttonDown(const QString& button, QString* error)
{
#ifdef Q_OS_WIN
    DWORD down = 0;
    DWORD up = 0;
    if (!buttonFlags(button, &down, &up, error)) {
        return false;
    }
    if (!sendMouseFlag(down)) {
        if (error) {
            *error = QStringLiteral("SendInput mouse down failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(button);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::buttonUp(const QString& button, QString* error)
{
#ifdef Q_OS_WIN
    DWORD down = 0;
    DWORD up = 0;
    if (!buttonFlags(button, &down, &up, error)) {
        return false;
    }
    if (!sendMouseFlag(up)) {
        if (error) {
            *error = QStringLiteral("SendInput mouse up failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(button);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::moveBy(int dx, int dy, QString* error)
{
#ifdef Q_OS_WIN
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        if (error) {
            *error = QStringLiteral("SendInput mouse move failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(dx);
    Q_UNUSED(dy);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::moveTo(int screenX, int screenY, QString* error)
{
    // Callers pass Qt logical global coordinates (same as GazePoint / QCursor::pos).
    // Do not mix with GetSystemMetrics physical virtual-desktop pixels.
    Q_UNUSED(error);
    QCursor::setPos(screenX, screenY);
    return true;
}

bool MouseInjector::scrollDelta(int wheelDelta, QString* error)
{
#ifdef Q_OS_WIN
    if (wheelDelta == 0) {
        return true;
    }
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(wheelDelta);
    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        if (error) {
            *error = QStringLiteral("SendInput mouse scroll failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(wheelDelta);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::scrollHorizontalDelta(int wheelDelta, QString* error)
{
#ifdef Q_OS_WIN
    if (wheelDelta == 0) {
        return true;
    }
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    in.mi.mouseData = static_cast<DWORD>(wheelDelta);
    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        if (error) {
            *error = QStringLiteral("SendInput horizontal scroll failed");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(wheelDelta);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::scroll(int notches, QString* error)
{
#ifdef Q_OS_WIN
    return scrollDelta(notches * WHEEL_DELTA, error);
#else
    Q_UNUSED(notches);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::scrollHorizontal(int notches, QString* error)
{
#ifdef Q_OS_WIN
    return scrollHorizontalDelta(notches * WHEEL_DELTA, error);
#else
    Q_UNUSED(notches);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

} // namespace gazer
