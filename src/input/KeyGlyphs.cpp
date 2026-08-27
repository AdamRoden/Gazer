#include "input/KeyGlyphs.h"

namespace gazer {
namespace KeyGlyphs {

namespace {

struct Pair {
    char32_t unshifted;
    char32_t shifted;
    const char* vk;
};

const Pair kPairs[] = {
    {'`', '~', "Oem3"},      {'1', '!', "1"},          {'2', '@', "2"},
    {'3', '#', "3"},         {'4', '$', "4"},          {'5', '%', "5"},
    {'6', '^', "6"},         {'7', '&', "7"},          {'8', '*', "8"},
    {'9', '(', "9"},         {'0', ')', "0"},          {'-', '_', "OemMinus"},
    {'=', '+', "OemPlus"},   {'[', '{', "Oem4"},       {']', '}', "Oem6"},
    {';', ':', "Oem1"},      {'\'', '"', "Oem7"},      {'\\', '|', "Oem5"},
    {',', '<', "OemComma"},  {'.', '>', "OemPeriod"},  {'/', '?', "Oem2"},
};

const Pair* findPair(QChar c)
{
    const char32_t u = c.unicode();
    for (const Pair& p : kPairs) {
        if (p.unshifted == u || p.shifted == u) {
            return &p;
        }
    }
    return nullptr;
}

QChar shiftedChar(QChar c)
{
    if (c.isLetter()) {
        return c.toUpper();
    }
    if (const Pair* p = findPair(c)) {
        return QChar(p->shifted);
    }
    return c;
}

bool isTypableLabel(const QString& label, const QString& sendKey)
{
    const QString l = label.trimmed();
    if (l.isEmpty() || l.size() > 2) {
        return false;
    }
    if (l.compare(sendKey, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (sendKey.size() != 1) {
        return false;
    }
    const QChar ch = sendKey[0];
    if (l.compare(QString(ch), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (const Pair* p = findPair(ch)) {
        return l == QString(QChar(p->unshifted)) || l == QString(QChar(p->shifted));
    }
    return false;
}

} // namespace

QString shiftedGlyph(const QString& sendKey)
{
    if (sendKey.size() != 1) {
        return {};
    }
    return QString(shiftedChar(sendKey[0]));
}

Stroke strokeForSend(const QString& sendKey, bool osShiftHeld)
{
    Stroke out;
    out.key = sendKey;
    if (sendKey.size() != 1) {
        return out;
    }
    const QChar c = sendKey[0];
    if (c.isLetter() && c.unicode() < 128) {
        return out;
    }
    if (const Pair* p = findPair(c)) {
        out.key = QString::fromLatin1(p->vk);
        out.extraShift = (c.unicode() == p->shifted) && !osShiftHeld;
        return out;
    }
    return out;
}

QString displayLabel(const QString& label, const QString& sendKey, bool shiftHeld)
{
    if (!shiftHeld || sendKey.isEmpty()) {
        return label;
    }
    const QString glyph = shiftedGlyph(sendKey);
    if (glyph.isEmpty() || !isTypableLabel(label, sendKey)) {
        return label;
    }
    return glyph;
}

} // namespace KeyGlyphs
} // namespace gazer
