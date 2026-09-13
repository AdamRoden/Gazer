#include "input/MouseInjector.h"

#include "utils/Log.h"
#include "utils/WinOverlay.h"

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

int vkForDownFlag(DWORD down)
{
    if (down == MOUSEEVENTF_LEFTDOWN) {
        return VK_LBUTTON;
    }
    if (down == MOUSEEVENTF_RIGHTDOWN) {
        return VK_RBUTTON;
    }
    if (down == MOUSEEVENTF_MIDDLEDOWN) {
        return VK_MBUTTON;
    }
    return 0;
}

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

bool sendMouseInputs(INPUT* inputs, UINT count)
{
    return SendInput(count, inputs, sizeof(INPUT)) == count;
}

bool sendMouseFlag(DWORD flag, DWORD data = 0)
{
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    in.mi.mouseData = data;
    return sendMouseInputs(&in, 1);
}

bool clickOnce(DWORD down, DWORD up, QString* error)
{
    INPUT inputs[3]{};
    UINT n = 0;
    if (const int vk = vkForDownFlag(down); vk != 0 && (GetAsyncKeyState(vk) & 0x8000)) {
        inputs[n].type = INPUT_MOUSE;
        inputs[n].mi.dwFlags = up;
        ++n;
    }
    inputs[n].type = INPUT_MOUSE;
    inputs[n].mi.dwFlags = down;
    ++n;
    inputs[n].type = INPUT_MOUSE;
    inputs[n].mi.dwFlags = up;
    ++n;
    if (!sendMouseInputs(inputs, n)) {
        if (error) {
            *error = QStringLiteral("SendInput mouse click failed");
        }
        return false;
    }
    return true;
}

bool sendWheelRaw(int horizontal, int vertical, QString* error)
{
    INPUT in[2]{};
    UINT n = 0;
    if (vertical != 0) {
        in[n].type = INPUT_MOUSE;
        in[n].mi.dwFlags = MOUSEEVENTF_WHEEL;
        in[n].mi.mouseData = static_cast<DWORD>(vertical);
        ++n;
    }
    if (horizontal != 0) {
        in[n].type = INPUT_MOUSE;
        in[n].mi.dwFlags = MOUSEEVENTF_HWHEEL;
        in[n].mi.mouseData = static_cast<DWORD>(horizontal);
        ++n;
    }
    if (n == 0) {
        return true;
    }
    if (!sendMouseInputs(in, n)) {
        if (error) {
            *error = QStringLiteral("SendInput mouse scroll failed");
        }
        return false;
    }
    return true;
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
    OverlayInputPassThrough pass;
    return clickOnce(down, up, error);
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
    DWORD down = 0;
    DWORD up = 0;
    if (!buttonFlags(button, &down, &up, error)) {
        return false;
    }
    OverlayInputPassThrough pass;
    return clickOnce(down, up, error) && clickOnce(down, up, error);
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
    OverlayInputPassThrough pass;
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
    OverlayInputPassThrough pass;
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
    OverlayInputPassThrough pass;
    return sendWheelRaw(0, wheelDelta, error);
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
    OverlayInputPassThrough pass;
    return sendWheelRaw(wheelDelta, 0, error);
#else
    Q_UNUSED(wheelDelta);
    if (error) {
        *error = QStringLiteral("Mouse injection only supported on Windows");
    }
    return false;
#endif
}

bool MouseInjector::scrollWheelRaw(int horizontal, int vertical, QString* error)
{
#ifdef Q_OS_WIN
    return sendWheelRaw(horizontal, vertical, error);
#else
    Q_UNUSED(horizontal);
    Q_UNUSED(vertical);
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
