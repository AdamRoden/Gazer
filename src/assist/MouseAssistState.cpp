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

bool MouseAssistState::toggleButton(const QString& button, bool& held, QString* error)
{
    InputOutput o;
    o.button = button;
    if (held) {
        o.type = InputOutput::Type::MouseUp;
        held = false;
    } else {
        o.type = InputOutput::Type::MouseDown;
        held = true;
    }
    const bool ok = inject(o, error);
    if (ok) {
        emit holdsChanged();
    }
    return ok;
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
    if (m_leftHeld) {
        (void)toggleButton(QStringLiteral("left"), m_leftHeld, &err);
    }
    if (m_rightHeld) {
        (void)toggleButton(QStringLiteral("right"), m_rightHeld, &err);
    }
    if (m_middleHeld) {
        (void)toggleButton(QStringLiteral("middle"), m_middleHeld, &err);
    }
}

void MouseAssistState::registerCommands(CommandRegistry& commands)
{
    commands.registerBuiltin(QStringLiteral("mouseLeftDownUp"), [this](QString* e) {
        return toggleButton(QStringLiteral("left"), m_leftHeld, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseRightDownUp"), [this](QString* e) {
        return toggleButton(QStringLiteral("right"), m_rightHeld, e);
    });
    commands.registerBuiltin(QStringLiteral("mouseMiddleDownUp"), [this](QString* e) {
        return toggleButton(QStringLiteral("middle"), m_middleHeld, e);
    });

    commands.registerBuiltin(QStringLiteral("cycleMouseMoveAmount"), [this](QString*) {
        static const int kSteps[] = {1, 5, 10, 20, 50, 100};
        m_moveStepIndex = (m_moveStepIndex + 1) % 6;
        m_moveAmountPx = kSteps[m_moveStepIndex];
        GAZER_INFO << "Mouse move amount:" << m_moveAmountPx << "px";
        return true;
    });
    commands.registerBuiltin(QStringLiteral("cycleMouseScrollAmount"), [this](QString*) {
        static const int kSteps[] = {1, 3, 6, 10};
        m_scrollStepIndex = (m_scrollStepIndex + 1) % 4;
        m_scrollNotches = kSteps[m_scrollStepIndex];
        GAZER_INFO << "Mouse scroll amount:" << m_scrollNotches;
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
