#pragma once

#include <QString>
#include <QStringList>

namespace gazer {

/// Windows SendInput keyboard injection.
class KeyboardInjector {
public:
    /// Press and release a named key (e.g. "Backspace", "A", "Enter", "F5").
    [[nodiscard]] static bool tapKey(const QString& keyName, QString* error = nullptr);
    [[nodiscard]] static bool keyDown(const QString& keyName, QString* error = nullptr);
    [[nodiscard]] static bool keyUp(const QString& keyName, QString* error = nullptr);

    /// Hold modifiers in order, tap last key, release modifiers reverse.
    /// Example: ["Control", "A"] → Ctrl+A
    [[nodiscard]] static bool combo(const QStringList& keys, QString* error = nullptr);

    /// Type unicode text (KEYEVENTF_UNICODE).
    [[nodiscard]] static bool typeText(const QString& text, QString* error = nullptr);
};

} // namespace gazer
