#include "input/InputService.h"

#include "input/KeyboardInjector.h"
#include "input/MouseInjector.h"
#include "utils/Log.h"

namespace gazer {

InputService::InputService(QObject* parent)
    : QObject(parent)
{
}

bool InputService::execute(const InputOutput& output, QString* error)
{
    QString err;
    bool ok = false;
    QString summary;

    switch (output.type) {
    case InputOutput::Type::KeyTap:
        ok = KeyboardInjector::tapKey(output.key, &err);
        summary = QStringLiteral("keyTap %1").arg(output.key);
        break;
    case InputOutput::Type::KeyCombo:
        ok = KeyboardInjector::combo(QStringList(output.keys.begin(), output.keys.end()), &err);
        summary = QStringLiteral("keyCombo %1").arg(output.keys.join(QLatin1Char('+')));
        break;
    case InputOutput::Type::Text:
        ok = KeyboardInjector::typeText(output.value, &err);
        summary = QStringLiteral("text \"%1\"").arg(output.value);
        break;
    case InputOutput::Type::MouseClick:
        ok = MouseInjector::click(output.button.isEmpty() ? QStringLiteral("left") : output.button,
                                  &err);
        summary = QStringLiteral("mouseClick %1").arg(output.button);
        break;
    case InputOutput::Type::MouseDoubleClick:
        ok = MouseInjector::doubleClick(
            output.button.isEmpty() ? QStringLiteral("left") : output.button, &err);
        summary = QStringLiteral("mouseDoubleClick %1").arg(output.button);
        break;
    case InputOutput::Type::MouseDown:
        ok = MouseInjector::buttonDown(
            output.button.isEmpty() ? QStringLiteral("left") : output.button, &err);
        summary = QStringLiteral("mouseDown %1").arg(output.button);
        break;
    case InputOutput::Type::MouseUp:
        ok = MouseInjector::buttonUp(
            output.button.isEmpty() ? QStringLiteral("left") : output.button, &err);
        summary = QStringLiteral("mouseUp %1").arg(output.button);
        break;
    case InputOutput::Type::MouseMove:
        ok = MouseInjector::moveBy(output.dx, output.dy, &err);
        summary = QStringLiteral("mouseMove %1,%2").arg(output.dx).arg(output.dy);
        break;
    case InputOutput::Type::MouseMoveTo:
        ok = MouseInjector::moveTo(output.dx, output.dy, &err);
        summary = QStringLiteral("mouseMoveTo %1,%2").arg(output.dx).arg(output.dy);
        break;
    case InputOutput::Type::MouseScroll:
        ok = MouseInjector::scroll(output.notches, &err);
        summary = QStringLiteral("mouseScroll %1").arg(output.notches);
        break;
    case InputOutput::Type::MouseScrollH:
        ok = MouseInjector::scrollHorizontal(output.notches, &err);
        summary = QStringLiteral("mouseScrollH %1").arg(output.notches);
        break;
    case InputOutput::Type::GamepadButton:
        ok = m_gamepad.pressButton(output.button, &err);
        if (ok) {
            ok = m_gamepad.releaseButton(output.button, &err);
        }
        summary = QStringLiteral("gamepadButton %1").arg(output.button);
        break;
    case InputOutput::Type::GamepadAxis:
        ok = m_gamepad.setAxis(output.axis, output.axisValue, &err);
        summary = QStringLiteral("gamepadAxis %1=%2").arg(output.axis).arg(output.axisValue);
        break;
    case InputOutput::Type::Unknown:
        err = QStringLiteral("Unknown input output type");
        ok = false;
        break;
    }

    if (!ok) {
        GAZER_WARN << "InputService failed:" << err;
        if (error) {
            *error = err;
        }
        emit failed(err);
        return false;
    }

    GAZER_INFO << "InputService:" << summary;
    emit executed(summary);
    return true;
}

bool InputService::executeAll(const QVector<InputOutput>& outputs, QString* error)
{
    for (const InputOutput& o : outputs) {
        if (!execute(o, error)) {
            return false;
        }
    }
    return true;
}

} // namespace gazer
