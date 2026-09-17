#pragma once

#include <QObject>

namespace gazer {

/// Exclusive assist mode + gaze-routing policy (single owner for board/aim rules).
class AssistSession final : public QObject {
    Q_OBJECT

public:
    enum class Mode {
        None,
        LookToScroll,
        LookToScrollPlaceCursor,
        MouseDwell,
        MagPickPoint,
        GazeFollow,
        ComboMouse,
        ComboMousePlaceCursor
    };

    explicit AssistSession(QObject* parent = nullptr);

    [[nodiscard]] Mode mode() const { return m_mode; }
    [[nodiscard]] bool isNone() const { return m_mode == Mode::None; }

    /// Full-screen aim: leave boards, free dock, pause background assist (LTS/follow).
    [[nodiscard]] bool freesScreenForAim() const;
    /// Pause gaze→mouse follow while aiming or an assist overlay owns the cursor.
    [[nodiscard]] bool pausesGazeFollow() const;
    /// ComboMouse wheel, mag-pick window, or LTS pie owns the sample; boards must not dwell it.
    [[nodiscard]] bool overlayHasGazePriority() const;

    /// Modes that share MouseDwellMove (direct / place-cursor / mag-pick).
    [[nodiscard]] static bool isMouseDwellFamily(Mode m);
    [[nodiscard]] static bool sameMouseDwellFamily(Mode a, Mode b);

    void enter(Mode mode);
    void leave(Mode mode);

signals:
    void modeChanged(Mode mode);
    /// @p next is the mode being entered (None when clearing).
    void leaving(Mode left, Mode next);

private:
    Mode m_mode = Mode::None;
};

} // namespace gazer
