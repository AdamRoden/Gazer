#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace gazer {

/// OptiKey-style hold for modifiers: one-shot Down auto-releases after a standard key.
enum class KeyHoldState {
    Up,
    Down,
    LockedDown
};

/// Cycles modifier keys Up → Down → LockedDown → Up and injects OS key-down/up.
class KeyStateManager final : public QObject {
    Q_OBJECT

public:
    /// @p down true = key-down, false = key-up.
    using InjectFn = std::function<bool(const QString& keyName, bool down, QString* error)>;

    explicit KeyStateManager(QObject* parent = nullptr);

    void setInjector(InjectFn fn);

    /// Canonical slot: "shift", "ctrl", "alt", "win". Empty if not a modifier.
    [[nodiscard]] static QString canonicalModifier(const QString& keyName);
    [[nodiscard]] static bool isModifier(const QString& keyName);
    [[nodiscard]] static QString injectNameFor(const QString& canonical);

    [[nodiscard]] KeyHoldState state(const QString& keyName) const;
    [[nodiscard]] bool isHeld(const QString& keyName) const;
    [[nodiscard]] bool isLocked(const QString& keyName) const;
    [[nodiscard]] bool anyHeld() const;
    [[nodiscard]] QStringList heldCanonical() const;

    /// Modifier tap: Up→Down→LockedDown→Up. Standard key: tap then release one-shots.
    [[nodiscard]] bool activate(const QString& keyName, QString* error = nullptr);
    [[nodiscard]] bool cycle(const QString& keyName, QString* error = nullptr);

    [[nodiscard]] bool down(const QString& keyName, QString* error = nullptr);
    [[nodiscard]] bool up(const QString& keyName, QString* error = nullptr);
    /// Down now, up + one-shot release after @p durationMs. No-op duration uses activate.
    [[nodiscard]] bool hold(const QString& keyName, int durationMs, QString* error = nullptr);
    [[nodiscard]] bool combo(const QStringList& keys, QString* error = nullptr);

    /// Release Down (not LockedDown) modifiers after a standard-key activation.
    [[nodiscard]] bool releaseOneShot(QString* error = nullptr);
    [[nodiscard]] bool releaseAll(QString* error = nullptr);

signals:
    void stateChanged();

private:
    struct Slot {
        KeyHoldState state = KeyHoldState::Up;
        QString injectName;
    };

    [[nodiscard]] bool inject(const QString& keyName, bool down, QString* error) const;
    [[nodiscard]] bool injectStroke(const QString& keyName, bool down, QString* error);
    [[nodiscard]] bool tapNamed(const QString& keyName, QString* error);
    [[nodiscard]] bool pushTransientShift(QString* error);
    [[nodiscard]] bool popTransientShift(QString* error);
    [[nodiscard]] bool setSlot(const QString& canonical, KeyHoldState next, const QString& injectName,
                               QString* error);
    [[nodiscard]] bool reassertLocked(QString* error);
    void emitIfChanged(bool changed);

    InjectFn m_inject;
    QHash<QString, Slot> m_slots;
    int m_transientShiftRefs = 0;
};

} // namespace gazer
