#pragma once

#include <QString>

namespace gazer {
namespace KeyNames {

[[nodiscard]] inline QString normalized(QString name)
{
    name = name.trimmed().toLower();
    name.remove(QLatin1Char(' '));
    name.remove(QLatin1Char('_'));
    name.remove(QLatin1Char('-'));
    return name;
}

[[nodiscard]] inline QString canonicalModifier(const QString& keyName)
{
    const QString n = normalized(keyName);
    static const struct {
        const char* alias;
        const char* id;
    } k[] = {
        {"shift", "shift"},       {"lshift", "shift"},      {"rshift", "shift"},
        {"leftshift", "shift"},   {"rightshift", "shift"},  {"control", "ctrl"},
        {"ctrl", "ctrl"},         {"lcontrol", "ctrl"},     {"rcontrol", "ctrl"},
        {"lctrl", "ctrl"},        {"rctrl", "ctrl"},        {"leftctrl", "ctrl"},
        {"rightctrl", "ctrl"},    {"leftcontrol", "ctrl"},  {"rightcontrol", "ctrl"},
        {"alt", "alt"},           {"menu", "alt"},          {"lalt", "alt"},
        {"ralt", "alt"},          {"lmenu", "alt"},         {"rmenu", "alt"},
        {"leftalt", "alt"},       {"rightalt", "alt"},      {"win", "win"},
        {"lwin", "win"},          {"rwin", "win"},          {"windows", "win"},
        {"leftwin", "win"},       {"rightwin", "win"},      {"meta", "win"},
        {"lmeta", "win"},         {"rmeta", "win"},
    };
    for (const auto& e : k) {
        if (n == QLatin1String(e.alias)) {
            return QString::fromLatin1(e.id);
        }
    }
    return {};
}

[[nodiscard]] inline bool isModifier(const QString& keyName)
{
    return !canonicalModifier(keyName).isEmpty();
}

[[nodiscard]] inline QString injectNameFor(const QString& canonical)
{
    if (canonical == QLatin1String("shift")) {
        return QStringLiteral("Shift");
    }
    if (canonical == QLatin1String("ctrl")) {
        return QStringLiteral("Control");
    }
    if (canonical == QLatin1String("alt")) {
        return QStringLiteral("Alt");
    }
    if (canonical == QLatin1String("win")) {
        return QStringLiteral("LWin");
    }
    return {};
}

} // namespace KeyNames
} // namespace gazer
