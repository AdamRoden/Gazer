#include "input/KeyboardInjector.h"

#include "input/KeyGlyphs.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

namespace {

#ifdef Q_OS_WIN

// Cluster keys share VKs with the numpad. KEYEVENTF_EXTENDEDKEY is what
// distinguishes them; without it Shift+Up is Shift+Numpad8.
struct VkStroke {
    WORD vk = 0;
    bool extended = false;
};

VkStroke namedStroke(QString name)
{
    name = name.trimmed();
    const QString u = name.toUpper();

    static const struct {
        const char* n;
        WORD vk;
        bool extended;
    } table[] = {
        {"BACK", VK_BACK, false},       {"BACKSPACE", VK_BACK, false},
        {"RETURN", VK_RETURN, false},   {"ENTER", VK_RETURN, false},
        {"TAB", VK_TAB, false},         {"ESCAPE", VK_ESCAPE, false},
        {"ESC", VK_ESCAPE, false},      {"SPACE", VK_SPACE, false},
        {"DELETE", VK_DELETE, true},    {"DEL", VK_DELETE, true},
        {"INSERT", VK_INSERT, true},    {"HOME", VK_HOME, true},
        {"END", VK_END, true},          {"PRIOR", VK_PRIOR, true},
        {"PAGEUP", VK_PRIOR, true},     {"NEXT", VK_NEXT, true},
        {"PAGEDOWN", VK_NEXT, true},    {"LEFT", VK_LEFT, true},
        {"RIGHT", VK_RIGHT, true},      {"UP", VK_UP, true},
        {"DOWN", VK_DOWN, true},
        {"CONTROL", VK_CONTROL, false}, {"CTRL", VK_CONTROL, false},
        {"LCONTROL", VK_LCONTROL, false}, {"RCONTROL", VK_RCONTROL, true},
        {"SHIFT", VK_SHIFT, false},     {"LSHIFT", VK_LSHIFT, false},
        {"RSHIFT", VK_RSHIFT, false},   {"ALT", VK_MENU, false},
        {"MENU", VK_MENU, false},       {"LMENU", VK_LMENU, false},
        {"RMENU", VK_RMENU, true},      {"LALT", VK_LMENU, false},
        {"RALT", VK_RMENU, true},
        {"LWIN", VK_LWIN, true},        {"RWIN", VK_RWIN, true},
        {"WIN", VK_LWIN, true},
        {"OEMMINUS", VK_OEM_MINUS, false}, {"OEM_MINUS", VK_OEM_MINUS, false},
        {"OEMPLUS", VK_OEM_PLUS, false},   {"OEM_PLUS", VK_OEM_PLUS, false},
        {"OEMCOMMA", VK_OEM_COMMA, false}, {"OEM_COMMA", VK_OEM_COMMA, false},
        {"OEMPERIOD", VK_OEM_PERIOD, false}, {"OEM_PERIOD", VK_OEM_PERIOD, false},
        {"OEM1", VK_OEM_1, false},      {"OEM_1", VK_OEM_1, false},
        {"OEM2", VK_OEM_2, false},      {"OEM_2", VK_OEM_2, false},
        {"OEM3", VK_OEM_3, false},      {"OEM_3", VK_OEM_3, false},
        {"OEM4", VK_OEM_4, false},      {"OEM_4", VK_OEM_4, false},
        {"OEM5", VK_OEM_5, false},      {"OEM_5", VK_OEM_5, false},
        {"OEM6", VK_OEM_6, false},      {"OEM_6", VK_OEM_6, false},
        {"OEM7", VK_OEM_7, false},      {"OEM_7", VK_OEM_7, false},
        {"F1", VK_F1, false},           {"F2", VK_F2, false},
        {"F3", VK_F3, false},           {"F4", VK_F4, false},
        {"F5", VK_F5, false},           {"F6", VK_F6, false},
        {"F7", VK_F7, false},           {"F8", VK_F8, false},
        {"F9", VK_F9, false},           {"F10", VK_F10, false},
        {"F11", VK_F11, false},         {"F12", VK_F12, false},
    };
    for (const auto& e : table) {
        if (u == QLatin1String(e.n)) {
            return {e.vk, e.extended};
        }
    }
    if (u.size() == 1) {
        const QChar c = u[0];
        if (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) {
            return {static_cast<WORD>(c.unicode()), false};
        }
        if (c >= QLatin1Char('0') && c <= QLatin1Char('9')) {
            return {static_cast<WORD>(c.unicode()), false};
        }
        const KeyGlyphs::Stroke st = KeyGlyphs::strokeForSend(name.trimmed(), false);
        if (st.key.compare(name.trimmed(), Qt::CaseInsensitive) != 0) {
            return namedStroke(st.key);
        }
    }
    return {};
}

bool sendVk(VkStroke stroke, bool keyUp)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = stroke.vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(stroke.vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (stroke.extended ? KEYEVENTF_EXTENDEDKEY : 0)
                    | (keyUp ? KEYEVENTF_KEYUP : 0);
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
    const VkStroke stroke = namedStroke(keyName);
    if (stroke.vk == 0) {
        if (error) {
            *error = QStringLiteral("Unknown key: %1").arg(keyName);
        }
        return false;
    }
    if (!sendVk(stroke, up)) {
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

bool KeyboardInjector::keyDown(const QString& keyName, QString* error)
{
#ifdef Q_OS_WIN
    if (namedStroke(keyName).vk != 0) {
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
    if (namedStroke(keyName).vk != 0) {
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
