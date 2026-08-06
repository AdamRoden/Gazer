#pragma once

#include <QString>

namespace gazer {

/// Virtual XInput-style gamepad backend.
/// Phase 3: logs intents. Phase 3+ can load ViGEmClient without changing callers.
class VirtualGamepad {
public:
    VirtualGamepad() = default;

    /// Connect / create virtual pad. Returns false if unavailable (stub always "ok" as log-only).
    [[nodiscard]] bool ensureConnected(QString* error = nullptr);

    [[nodiscard]] bool pressButton(const QString& button, QString* error = nullptr);
    [[nodiscard]] bool releaseButton(const QString& button, QString* error = nullptr);
    /// axis: lx ly rx ry lt rt — value in [-1, 1] (triggers 0..1 accepted).
    [[nodiscard]] bool setAxis(const QString& axis, double value, QString* error = nullptr);

    [[nodiscard]] bool isConnected() const { return m_connected; }
    [[nodiscard]] QString backendName() const { return m_backend; }

private:
    bool m_connected = false;
    QString m_backend = QStringLiteral("stub");
};

} // namespace gazer
