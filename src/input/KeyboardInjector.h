#pragma once

#include <QString>

namespace gazer {

/// Windows SendInput keyboard injection.
class KeyboardInjector {
public:
    [[nodiscard]] static bool keyDown(const QString& keyName, QString* error = nullptr);
    [[nodiscard]] static bool keyUp(const QString& keyName, QString* error = nullptr);

    /// Type unicode text (KEYEVENTF_UNICODE).
    [[nodiscard]] static bool typeText(const QString& text, QString* error = nullptr);
};

} // namespace gazer
