#include "assist/AssistSession.h"

#include "utils/Log.h"

namespace gazer {

AssistSession::AssistSession(QObject* parent)
    : QObject(parent)
{
}

bool AssistSession::freesScreenForAim() const
{
    return m_mode == Mode::MouseDwell || m_mode == Mode::MagPickPoint
           || m_mode == Mode::LookToScrollPlaceCursor
           || m_mode == Mode::ComboMousePlaceCursor;
}

bool AssistSession::pausesGazeFollow() const
{
    return freesScreenForAim() || overlayHasGazePriority();
}

bool AssistSession::overlayHasGazePriority() const
{
    return m_mode == Mode::ComboMouse || m_mode == Mode::MagPickPoint;
}

bool AssistSession::isMouseDwellFamily(Mode m)
{
    return m == Mode::MouseDwell || m == Mode::MagPickPoint
           || m == Mode::LookToScrollPlaceCursor || m == Mode::ComboMousePlaceCursor;
}

bool AssistSession::sameMouseDwellFamily(Mode a, Mode b)
{
    return isMouseDwellFamily(a) && isMouseDwellFamily(b);
}

void AssistSession::enter(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    const Mode prev = m_mode;
    if (prev != Mode::None) {
        emit leaving(prev, mode);
    }
    m_mode = mode;
    GAZER_INFO << "AssistSession enter" << int(mode);
    emit modeChanged(m_mode);
}

void AssistSession::leave(Mode mode)
{
    if (m_mode != mode) {
        return;
    }
    m_mode = Mode::None;
    emit leaving(mode, Mode::None);
    emit modeChanged(m_mode);
    GAZER_INFO << "AssistSession leave" << int(mode);
}

} // namespace gazer
