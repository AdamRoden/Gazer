#include "input/VirtualGamepad.h"

namespace gazer {

namespace {

bool failUnavailable(QString* error)
{
    if (error) {
        *error = QStringLiteral("Virtual gamepad is not available (ViGEm not built)");
    }
    return false;
}

} // namespace

bool VirtualGamepad::ensureConnected(QString* error)
{
    return failUnavailable(error);
}

bool VirtualGamepad::pressButton(const QString& button, QString* error)
{
    Q_UNUSED(button);
    return failUnavailable(error);
}

bool VirtualGamepad::releaseButton(const QString& button, QString* error)
{
    Q_UNUSED(button);
    return failUnavailable(error);
}

bool VirtualGamepad::setAxis(const QString& axis, double value, QString* error)
{
    Q_UNUSED(axis);
    Q_UNUSED(value);
    return failUnavailable(error);
}

} // namespace gazer
