#include "input/KeyStateManager.h"

#include "input/KeyGlyphs.h"
#include "input/KeyNames.h"

#include <QTimer>

namespace gazer {

KeyStateManager::KeyStateManager(QObject* parent)
    : QObject(parent)
{
}

void KeyStateManager::setInjector(InjectFn fn)
{
    m_inject = std::move(fn);
}

QString KeyStateManager::canonicalModifier(const QString& keyName)
{
    return KeyNames::canonicalModifier(keyName);
}

bool KeyStateManager::isModifier(const QString& keyName)
{
    return KeyNames::isModifier(keyName);
}

QString KeyStateManager::injectNameFor(const QString& canonical)
{
    return KeyNames::injectNameFor(canonical);
}

KeyHoldState KeyStateManager::state(const QString& keyName) const
{
    const QString id = canonicalModifier(keyName);
    if (id.isEmpty()) {
        return KeyHoldState::Up;
    }
    return m_slots.value(id).state;
}

bool KeyStateManager::isHeld(const QString& keyName) const
{
    const KeyHoldState s = state(keyName);
    return s == KeyHoldState::Down || s == KeyHoldState::LockedDown;
}

bool KeyStateManager::isLocked(const QString& keyName) const
{
    return state(keyName) == KeyHoldState::LockedDown;
}

bool KeyStateManager::anyHeld() const
{
    for (auto it = m_slots.cbegin(); it != m_slots.cend(); ++it) {
        if (it.value().state != KeyHoldState::Up) {
            return true;
        }
    }
    return false;
}

QStringList KeyStateManager::heldCanonical() const
{
    QStringList out;
    for (auto it = m_slots.cbegin(); it != m_slots.cend(); ++it) {
        if (it.value().state != KeyHoldState::Up) {
            out.push_back(it.key());
        }
    }
    out.sort();
    return out;
}

bool KeyStateManager::inject(const QString& keyName, bool down, QString* error) const
{
    if (!m_inject) {
        if (error) {
            *error = QStringLiteral("No key injector");
        }
        return false;
    }
    return m_inject(keyName, down, error);
}

bool KeyStateManager::pushTransientShift(QString* error)
{
    if (m_transientShiftRefs == 0) {
        if (!setSlot(QStringLiteral("shift"), KeyHoldState::Down, QStringLiteral("Shift"), error)) {
            return false;
        }
        emitIfChanged(true);
    }
    ++m_transientShiftRefs;
    return true;
}

bool KeyStateManager::popTransientShift(QString* error)
{
    if (m_transientShiftRefs <= 0) {
        return true;
    }
    --m_transientShiftRefs;
    if (m_transientShiftRefs > 0) {
        return true;
    }
    if (state(QStringLiteral("shift")) != KeyHoldState::Down) {
        return true;
    }
    if (!setSlot(QStringLiteral("shift"), KeyHoldState::Up, QStringLiteral("Shift"), error)) {
        ++m_transientShiftRefs;
        return false;
    }
    emitIfChanged(true);
    return true;
}

bool KeyStateManager::injectStroke(const QString& keyName, bool down, QString* error)
{
    const KeyGlyphs::Stroke phys = KeyGlyphs::strokeForSend(keyName, false);
    if (down) {
        const bool extra = phys.extraShift && !isHeld(QStringLiteral("shift"));
        if (extra && !pushTransientShift(error)) {
            return false;
        }
        if (!inject(phys.key, true, error)) {
            if (extra) {
                QString ignored;
                (void)popTransientShift(&ignored);
            }
            return false;
        }
        return true;
    }
    if (!inject(phys.key, false, error)) {
        return false;
    }
    if (phys.extraShift && m_transientShiftRefs > 0) {
        return popTransientShift(error);
    }
    return true;
}

bool KeyStateManager::tapNamed(const QString& keyName, QString* error)
{
    return injectStroke(keyName, true, error) && injectStroke(keyName, false, error);
}

void KeyStateManager::emitIfChanged(bool changed)
{
    if (changed) {
        emit stateChanged();
    }
}

bool KeyStateManager::setSlot(const QString& canonical, KeyHoldState next, const QString& injectName,
                              QString* error)
{
    Slot& slot = m_slots[canonical];
    const KeyHoldState prev = slot.state;
    if (prev == next) {
        if (slot.injectName.isEmpty()) {
            slot.injectName = injectName;
        }
        return true;
    }

    const bool wasHeld = prev != KeyHoldState::Up;
    const bool willHold = next != KeyHoldState::Up;
    QString name = slot.injectName.isEmpty() ? injectName : slot.injectName;
    if (name.isEmpty()) {
        name = injectNameFor(canonical);
    }

    if (!wasHeld && willHold) {
        if (!inject(name, true, error)) {
            return false;
        }
    } else if (wasHeld && !willHold) {
        if (!inject(name, false, error)) {
            return false;
        }
    }

    slot.state = next;
    slot.injectName = willHold ? name : QString();
    return true;
}

bool KeyStateManager::reassertLocked(QString* error)
{
    bool ok = true;
    for (auto it = m_slots.begin(); it != m_slots.end(); ++it) {
        Slot& slot = it.value();
        if (slot.state != KeyHoldState::LockedDown) {
            continue;
        }
        QString name = slot.injectName.isEmpty() ? injectNameFor(it.key()) : slot.injectName;
        if (!inject(name, true, error)) {
            ok = false;
        }
    }
    return ok;
}

bool KeyStateManager::cycle(const QString& keyName, QString* error)
{
    const QString id = canonicalModifier(keyName);
    if (id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Not a modifier: %1").arg(keyName);
        }
        return false;
    }
    const QString injectName = keyName.trimmed().isEmpty() ? injectNameFor(id) : keyName.trimmed();
    const KeyHoldState cur = m_slots.value(id).state;
    KeyHoldState next = KeyHoldState::Down;
    if (cur == KeyHoldState::Down) {
        next = KeyHoldState::LockedDown;
    } else if (cur == KeyHoldState::LockedDown) {
        next = KeyHoldState::Up;
    }
    if (!setSlot(id, next, injectName, error)) {
        return false;
    }
    emitIfChanged(cur != next);
    return true;
}

bool KeyStateManager::activate(const QString& keyName, QString* error)
{
    if (isModifier(keyName)) {
        return cycle(keyName, error);
    }
    if (!tapNamed(keyName, error)) {
        return false;
    }
    return releaseOneShot(error);
}

bool KeyStateManager::down(const QString& keyName, QString* error)
{
    const QString id = canonicalModifier(keyName);
    if (id.isEmpty()) {
        return injectStroke(keyName, true, error);
    }
    const KeyHoldState cur = m_slots.value(id).state;
    if (cur != KeyHoldState::Up) {
        return true;
    }
    if (!setSlot(id, KeyHoldState::Down, keyName.trimmed(), error)) {
        return false;
    }
    emitIfChanged(true);
    return true;
}

bool KeyStateManager::up(const QString& keyName, QString* error)
{
    const QString id = canonicalModifier(keyName);
    if (id.isEmpty()) {
        return injectStroke(keyName, false, error);
    }
    const KeyHoldState cur = m_slots.value(id).state;
    if (cur == KeyHoldState::Up) {
        return true;
    }
    if (!setSlot(id, KeyHoldState::Up, keyName.trimmed(), error)) {
        return false;
    }
    emitIfChanged(true);
    return true;
}

bool KeyStateManager::hold(const QString& keyName, int durationMs, QString* error)
{
    if (durationMs <= 0) {
        return activate(keyName, error);
    }
    if (!down(keyName, error)) {
        return false;
    }
    const QString key = keyName;
    QTimer::singleShot(durationMs, this, [this, key]() {
        QString ignored;
        (void)up(key, &ignored);
        (void)releaseOneShot(&ignored);
    });
    return true;
}

bool KeyStateManager::combo(const QStringList& keys, QString* error)
{
    if (keys.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty key combo");
        }
        return false;
    }
    for (const QString& k : keys) {
        if (!injectStroke(k, true, error)) {
            return false;
        }
    }
    for (int i = keys.size() - 1; i >= 0; --i) {
        if (!injectStroke(keys[i], false, error)) {
            return false;
        }
    }
    if (!releaseOneShot(error)) {
        return false;
    }
    return reassertLocked(error);
}

bool KeyStateManager::releaseOneShot(QString* error)
{
    bool changed = false;
    bool ok = true;
    QString lastErr;
    const QStringList ids = m_slots.keys();
    for (const QString& id : ids) {
        if (m_slots.value(id).state != KeyHoldState::Down) {
            continue;
        }
        QString err;
        if (!setSlot(id, KeyHoldState::Up, m_slots.value(id).injectName, &err)) {
            ok = false;
            lastErr = err;
            continue;
        }
        changed = true;
    }
    if (!ok && error) {
        *error = lastErr;
    }
    emitIfChanged(changed);
    return ok;
}

bool KeyStateManager::releaseAll(QString* error)
{
    bool changed = false;
    bool ok = true;
    QString lastErr;
    const QStringList ids = m_slots.keys();
    for (const QString& id : ids) {
        if (m_slots.value(id).state == KeyHoldState::Up) {
            continue;
        }
        QString err;
        if (!setSlot(id, KeyHoldState::Up, m_slots.value(id).injectName, &err)) {
            ok = false;
            lastErr = err;
            continue;
        }
        changed = true;
    }
    if (!ok && error) {
        *error = lastErr;
    }
    emitIfChanged(changed);
    return ok;
}

} // namespace gazer
