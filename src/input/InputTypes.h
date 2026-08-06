#pragma once

#include <QString>
#include <QVector>

namespace gazer {

/// One atomic output step from a mapping profile or speak path.
struct InputOutput {
    enum class Type {
        KeyTap,          // single key press+release
        KeyCombo,        // modifiers + key
        Text,            // unicode string via keyboard
        MouseClick,      // left/right/middle at current cursor
        MouseDoubleClick,
        MouseDown,       // button hold (drag start)
        MouseUp,         // button release (drag end)
        MouseMove,       // relative dx,dy
        MouseMoveTo,     // absolute screen x,y via dx,dy fields
        MouseScroll,     // vertical wheel notches
        MouseScrollH,    // horizontal wheel notches
        GamepadButton,
        GamepadAxis,
        Unknown
    };

    Type type = Type::Unknown;
    QString key;           // keyTap / primary key for combo
    QVector<QString> keys; // keyCombo list (last is main key)
    QString value;         // text
    QString button;        // mouse or gamepad button name
    int dx = 0;
    int dy = 0;
    int notches = 0;       // scroll
    double axisValue = 0;  // -1..1 gamepad
    QString axis;          // "lx","ly","rx","ry","lt","rt"
};

} // namespace gazer
