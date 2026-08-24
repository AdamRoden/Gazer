#pragma once

#include "input/InputService.h"
#include "input/InputTypes.h"

#include <QObject>
#include <QString>

namespace gazer {

class CommandRegistry;

/// Toggle/cycle state for mouse assist; pure injectables live in mapping JSON.
class MouseAssistState final : public QObject {
    Q_OBJECT

public:
    explicit MouseAssistState(InputService& input, QObject* parent = nullptr);

    void registerCommands(CommandRegistry& commands);

    /// Release any held buttons (call on quit / shutdown / stop loops).
    void releaseAllHolds();

    /// Inject down or up only if the tracked hold state would change.
    [[nodiscard]] bool setHeld(const QString& button, bool down, QString* error = nullptr);

    /// Sync hold flags after a full click (down+up) without injecting.
    void markReleased(const QString& button);

    [[nodiscard]] bool isLeftHeld() const { return m_leftHeld; }
    [[nodiscard]] bool isRightHeld() const { return m_rightHeld; }
    [[nodiscard]] bool isMiddleHeld() const { return m_middleHeld; }
    [[nodiscard]] bool anyButtonHeld() const
    {
        return m_leftHeld || m_rightHeld || m_middleHeld;
    }

    [[nodiscard]] int moveAmountPx() const { return m_moveAmountPx; }
    [[nodiscard]] int scrollNotches() const { return m_scrollNotches; }

signals:
    void holdsChanged();
    void amountsChanged();

private:
    [[nodiscard]] bool inject(const InputOutput& o, QString* error);
    [[nodiscard]] bool* heldFlag(const QString& button);
    [[nodiscard]] bool nudge(int dx, int dy, QString* error);
    [[nodiscard]] bool moveToEdge(Qt::Alignment edge, QString* error);

    InputService& m_input;
    int m_moveAmountPx = 40;
    int m_moveStepIndex = 1;
    int m_scrollNotches = 3;
    int m_scrollStepIndex = 1;
    bool m_leftHeld = false;
    bool m_rightHeld = false;
    bool m_middleHeld = false;
};

} // namespace gazer
