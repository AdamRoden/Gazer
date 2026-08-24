#include "assist/MouseAssistState.h"

#include "app/CommandRegistry.h"
#include "utils/Log.h"

#include <QGuiApplication>
#include <QScreen>

namespace gazer {

MouseAssistState::MouseAssistState(InputService& input, QObject* parent)
    : QObject(parent)
    , m_input(input)
{
}

bool MouseAssistState::inject(const InputOutput& o, QString* error)
{
    return m_input.execute(o, error);
}

bool* MouseAssistState::heldFlag(const QString& button)
{
    const QString b = button.trimmed().toLower();
    if (b == QLatin1String("left") || b.isEmpty()) {
        return &m_leftHeld;
    }
    if (b == QLatin1String("right")) {
        return &m_rightHeld;
    }
    if (b == QLatin1String("middle")) {
        return &m_middleHeld;
    }
    return nullptr;
}

bool MouseAssistState::setHeld(const QString& button, bool down, QString* error)
{
    bool* flag = heldFlag(button);
    if (!flag) {
        if (error) {
            *error = QStringLiteral("Unknown mouse button: %1").arg(button);
        }
        return false;
    }
    if (*flag == down) {
        return true;
    }
    InputOutput o;
    o.button = button.trimmed().isEmpty() ? QStringLiteral("left") : button.trimmed();
    o.type = down ? InputOutput::Type::MouseDown : InputOutput::Type::MouseUp;
    if (!inject(o, error)) {
        return false;
    }
    *flag = down;
    emit holdsChanged();
    return true;
}

void MouseAssistState::markReleased(const QString& button)
{
    bool* flag = heldFlag(button);
    if (!flag || !*flag) {
        return;
    }
    *flag = false;
    emit holdsChanged();
}

bool MouseAssistState::nudge(int dx, int dy, QString* error)
{
    InputOutput o;
    o.type = InputOutput::Type::MouseMove;
    o.dx = dx;
    o.dy = dy;
    return inject(o, error);
}

bool MouseAssistState::moveToEdge(Qt::Alignment edge, QString* error)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        if (error) {
            *error = QStringLiteral("No primary screen");
        }
        return false;
    }
    const QRect g = screen->availableGeometry();
    int x = g.center().x();
    int y = g.center().y();
    if (edge & Qt::AlignLeft) {
        x = g.left() + 4;
    }
    if (edge & Qt::AlignRight) {
        x = g.right() - 4;
    }
    if (edge & Qt::AlignTop) {
        y = g.top() + 4;
    }
    if (edge & Qt::AlignBottom) {
        y = g.bottom() - 4;
    }
    InputOutput o;
    o.type = InputOutput::Type::MouseMoveTo;
    o.dx = x;
    o.dy = y;
    return inject(o, error);
}

void MouseAssistState::releaseAllHolds()
{
    QString err;
    (void)setHeld(QStringLiteral("left"), false, &err);
    (void)setHeld(QStringLiteral("right"), false, &err);
    (void)setHeld(QStringLiteral("middle"), false, &err);
}

void MouseAssistState::registerCommands(CommandRegistry& commands)
{
    commands.registerBuiltin(QStringLiteral("mouseLeftDownUp"), [this](QString* e) {
        return setHeld(QStringLiteral("left"), !m_leftHeld, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseRightDownUp"), [this](QString* e) {
        return setHeld(QStringLiteral("right"), !m_rightHeld, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMiddleDownUp"), [this](QString* e) {
        return setHeld(QStringLiteral("middle"), !m_middleHeld, e);
    });

    commands.registerBuiltin(QStringLiteral("cycleMouseMoveAmount"), [this](QString*) {
        static const int kSteps[] = {1, 5, 10, 20, 50, 100};
        m_moveStepIndex = (m_moveStepIndex + 1) % 6;
        m_moveAmountPx = kSteps[m_moveStepIndex];
        GAZER_INFO << "Mouse move amount:" << m_moveAmountPx << "px";
        emit amountsChanged();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("cycleMouseScrollAmount"), [this](QString*) {
        static const int kSteps[] = {1, 3, 6, 10};
        m_scrollStepIndex = (m_scrollStepIndex + 1) % 4;
        m_scrollNotches = kSteps[m_scrollStepIndex];
        GAZER_INFO << "Mouse scroll amount:" << m_scrollNotches;
        emit amountsChanged();
        return true;
    });

    commands.registerBuiltin(QStringLiteral("mouseMoveUp"), [this](QString* e) {
        return nudge(0, -m_moveAmountPx, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveDown"), [this](QString* e) {
        return nudge(0, m_moveAmountPx, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveLeft"), [this](QString* e) {
        return nudge(-m_moveAmountPx, 0, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveRight"), [this](QString* e) {
        return nudge(m_moveAmountPx, 0, e);
    });

    // Variable scroll amounts stay stateful; fixed-notch scroll is in mapping JSON.
    auto scrollV = [this](int dir, QString* error) {
        InputOutput o;
        o.type = InputOutput::Type::MouseScroll;
        o.notches = dir * m_scrollNotches;
        return inject(o, error);
    };
    auto scrollH = [this](int dir, QString* error) {
        InputOutput o;
        o.type = InputOutput::Type::MouseScrollH;
        o.notches = dir * m_scrollNotches;
        return inject(o, error);
    };
    commands.registerBuiltin(QStringLiteral("mouseScrollUp"),
                             [scrollV](QString* e) { return scrollV(1, e); });
    commands.registerBuiltin(QStringLiteral("mouseScrollDown"),
                             [scrollV](QString* e) { return scrollV(-1, e); });
    commands.registerBuiltin(QStringLiteral("mouseScrollLeft"),
                             [scrollH](QString* e) { return scrollH(-1, e); });
    commands.registerBuiltin(QStringLiteral("mouseScrollRight"),
                             [scrollH](QString* e) { return scrollH(1, e); });

    commands.registerBuiltin(QStringLiteral("mouseMoveToTop"), [this](QString* e) {
        return moveToEdge(Qt::AlignHCenter | Qt::AlignTop, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveToBottom"), [this](QString* e) {
        return moveToEdge(Qt::AlignHCenter | Qt::AlignBottom, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveToLeft"), [this](QString* e) {
        return moveToEdge(Qt::AlignLeft | Qt::AlignVCenter, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMoveToRight"), [this](QString* e) {
        return moveToEdge(Qt::AlignRight | Qt::AlignVCenter, e);
    });
}

} // namespace gazer
