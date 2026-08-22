#include "input/KeyboardInjector.h"

#include "utils/Log.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

namespace {

#ifdef Q_OS_WIN

WORD virtualKeyFromName(QString name)
{
    name = name.trimmed();
    const QString u = name.toUpper();

    static const struct {
        const char* n;
        WORD vk;
    } table[] = {
        {"BACK", VK_BACK},       {"BACKSPACE", VK_BACK}, {"RETURN", VK_RETURN},
        {"ENTER", VK_RETURN},    {"TAB", VK_TAB},        {"ESCAPE", VK_ESCAPE},
        {"ESC", VK_ESCAPE},      {"SPACE", VK_SPACE},    {"DELETE", VK_DELETE},
        {"DEL", VK_DELETE},      {"INSERT", VK_INSERT},  {"HOME", VK_HOME},
        {"END", VK_END},         {"PRIOR", VK_PRIOR},    {"PAGEUP", VK_PRIOR},
        {"NEXT", VK_NEXT},       {"PAGEDOWN", VK_NEXT},  {"LEFT", VK_LEFT},
        {"RIGHT", VK_RIGHT},     {"UP", VK_UP},          {"DOWN", VK_DOWN},
        {"CONTROL", VK_CONTROL}, {"CTRL", VK_CONTROL},   {"LCONTROL", VK_LCONTROL},
        {"RCONTROL", VK_RCONTROL}, {"SHIFT", VK_SHIFT},  {"LSHIFT", VK_LSHIFT},
        {"RSHIFT", VK_RSHIFT},   {"ALT", VK_MENU},       {"MENU", VK_MENU},
        {"LWIN", VK_LWIN},       {"RWIN", VK_RWIN},      {"WIN", VK_LWIN},
        {"F1", VK_F1},           {"F2", VK_F2},          {"F3", VK_F3},
        {"F4", VK_F4},           {"F5", VK_F5},          {"F6", VK_F6},
        {"F7", VK_F7},           {"F8", VK_F8},          {"F9", VK_F9},
        {"F10", VK_F10},         {"F11", VK_F11},        {"F12", VK_F12},
    };
    for (const auto& e : table) {
        if (u == QLatin1String(e.n)) {
            return e.vk;
        }
    }
    if (u.size() == 1) {
        const QChar c = u[0];
        if (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) {
            return static_cast<WORD>(c.unicode());
        }
        if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) {
            return static_cast<WORD>(c.unicode());
        }
    }
    return 0;
}

bool sendVk(WORD vk, bool keyUp)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool sendUnicode(wchar_t ch, bool keyUp)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = 0;
    in.ki.wScan = ch;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (keyUp ? KEYEVENTF_KEYUP : 0);
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

#endif

} // namespace

namespace {

bool sendNamed(const QString& keyName, bool up, QString* error)
{
#ifdef Q_OS_WIN
    const WORD vk = virtualKeyFromName(keyName);
    if (vk == 0) {
        if (error) {
            *error = QStringLiteral("Unknown key: %1").arg(keyName);
        }
        return false;
    }
    if (!sendVk(vk, up)) {
        if (error) {
            *error = QStringLiteral("SendInput failed for key %1").arg(keyName);
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(up);
    if (error) {
        *error = QStringLiteral("Keyboard injection only supported on Windows");
    }
    return false;
#endif
}

} // namespace

bool KeyboardInjector::tapKey(const QString& keyName, QString* error)
{
    return keyDown(keyName, error) && keyUp(keyName, error);
}

bool KeyboardInjector::keyDown(const QString& keyName, QString* error)
{
#ifdef Q_OS_WIN
    if (virtualKeyFromName(keyName) != 0) {
        return sendNamed(keyName, false, error);
    }
    if (keyName.size() == 1) {
        const wchar_t ch = static_cast<wchar_t>(keyName[0].unicode());
        if (!sendUnicode(ch, false)) {
            if (error) {
                *error = QStringLiteral("SendInput unicode failed");
            }
            return false;
        }
        return true;
    }
    return sendNamed(keyName, false, error);
#else
    return sendNamed(keyName, false, error);
#endif
}

bool KeyboardInjector::keyUp(const QString& keyName, QString* error)
{
#ifdef Q_OS_WIN
    if (virtualKeyFromName(keyName) != 0) {
        return sendNamed(keyName, true, error);
    }
    if (keyName.size() == 1) {
        const wchar_t ch = static_cast<wchar_t>(keyName[0].unicode());
        if (!sendUnicode(ch, true)) {
            if (error) {
                *error = QStringLiteral("SendInput unicode failed");
            }
            return false;
        }
        return true;
    }
    return sendNamed(keyName, true, error);
#else
    return sendNamed(keyName, true, error);
#endif
}

bool KeyboardInjector::combo(const QStringList& keys, QString* error)
{
#ifdef Q_OS_WIN
    if (keys.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty key combo");
        }
        return false;
    }
    QVector<WORD> vks;
    vks.reserve(keys.size());
    for (const QString& k : keys) {
        const WORD vk = virtualKeyFromName(k);
        if (vk == 0) {
            if (error) {
                *error = QStringLiteral("Unknown key in combo: %1").arg(k);
            }
            return false;
        }
        vks.push_back(vk);
    }
    for (WORD vk : vks) {
        if (!sendVk(vk, false)) {
            if (error) {
                *error = QStringLiteral("SendInput key-down failed");
            }
            return false;
        }
    }
    for (int i = vks.size() - 1; i >= 0; --i) {
        if (!sendVk(vks[i], true)) {
            if (error) {
                *error = QStringLiteral("SendInput key-up failed");
            }
            return false;
        }
    }
    return true;
#else
    Q_UNUSED(keys);
    if (error) {
        *error = QStringLiteral("Keyboard injection only supported on Windows");
    }
    return false;
#endif
}

bool KeyboardInjector::typeText(const QString& text, QString* error)
{
#ifdef Q_OS_WIN
    for (QChar qc : text) {
        const wchar_t ch = static_cast<wchar_t>(qc.unicode());
        if (!sendUnicode(ch, false) || !sendUnicode(ch, true)) {
            if (error) {
                *error = QStringLiteral("SendInput unicode failed");
            }
            return false;
        }
    }
    return true;
#else
    Q_UNUSED(text);
    if (error) {
        *error = QStringLiteral("Keyboard injection only supported on Windows");
    }
    return false;
#endif
}

} // namespace gazer
