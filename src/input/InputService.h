#pragma once

#include "input/InputTypes.h"
#include "input/VirtualGamepad.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

class KeyStateManager;

/// Facade over keyboard, mouse, and virtual gamepad injectors.
class InputService final : public QObject {
    Q_OBJECT

public:
    explicit InputService(KeyStateManager& keys, QObject* parent = nullptr);

    [[nodiscard]] bool execute(const InputOutput& output, QString* error = nullptr);
    [[nodiscard]] bool executeAll(const QVector<InputOutput>& outputs, QString* error = nullptr);

    [[nodiscard]] VirtualGamepad& gamepad() { return m_gamepad; }

signals:
    void executed(const QString& summary);
    void failed(const QString& error);

private:
    KeyStateManager& m_keys;
    VirtualGamepad m_gamepad;
};

} // namespace gazer
