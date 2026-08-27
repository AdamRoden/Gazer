#pragma once

#include <QString>

namespace gazer {
namespace KeyGlyphs {

/// US QWERTY overlay: what a send character types with Shift held.
[[nodiscard]] QString shiftedGlyph(const QString& sendKey);

/// Physical key name for KeyboardInjector. extraShift is true for a shifted
/// punctuation glyph when OS Shift is not already down. Letters are VKs.
struct Stroke {
    QString key;
    bool extraShift = false;
};

[[nodiscard]] Stroke strokeForSend(const QString& sendKey, bool osShiftHeld);

/// Rewrite a character-key label when Shift is held. Function keys are unchanged.
[[nodiscard]] QString displayLabel(const QString& label, const QString& sendKey, bool shiftHeld);

} // namespace KeyGlyphs
} // namespace gazer
