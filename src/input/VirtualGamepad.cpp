#include "input/VirtualGamepad.h"

#include "utils/Log.h"

namespace gazer {

bool VirtualGamepad::ensureConnected(QString* error)
{
    // TODO: ViGEmBus + ViGEmClient when third_party/vigem is present.
    if (!m_connected) {
        m_connected = true;
        m_backend = QStringLiteral("stub");
        GAZER_INFO << "VirtualGamepad: using stub backend (no ViGEm yet)";
    }
    Q_UNUSED(error);
    return true;
}

bool VirtualGamepad::pressButton(const QString& button, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    GAZER_INFO << "[gamepad stub] press" << button;
    return true;
}

bool VirtualGamepad::releaseButton(const QString& button, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    GAZER_INFO << "[gamepad stub] release" << button;
    return true;
}

bool VirtualGamepad::setAxis(const QString& axis, double value, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    GAZER_INFO << "[gamepad stub] axis" << axis << "=" << value;
    return true;
}

} // namespace gazer
