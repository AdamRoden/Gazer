#include "ui/PageHostWindow.h"

#include <QPointer>
#include <QQuickPaintedItem>
#include <QRunnable>

#include <functional>

namespace gazer {
namespace {

/// Second chance, then show the board even if no present arrived.
constexpr int kFrameHoldFallbackMs = 200;

class PostedJob final : public QRunnable {
public:
    explicit PostedJob(std::function<void()> fn)
        : m_fn(std::move(fn))
    {
    }

    void run() override
    {
        if (m_fn) {
            m_fn();
        }
    }

private:
    std::function<void()> m_fn;
};

} // namespace

void PageHostWindow::armFrameHold()
{
    m_hold.store(HoldArmed, std::memory_order_release);
    m_holdRetried = false;
    // Clip before the resize. applyPieceClip is the only setMask path.
    applyPieceClip();
    scheduleHoldRelease();
    m_holdTimer.start(kFrameHoldFallbackMs);
}

void PageHostWindow::scheduleHoldRelease()
{
    if (m_hold.load(std::memory_order_acquire) == HoldIdle) {
        return;
    }
    if (m_board) {
        m_board->update();
    }
    // AfterSwap of a frame that is already past sync still shows the old
    // scene. Release only once paint has marked this hold, and try the next
    // frame otherwise. The job hops back to the GUI thread before setMask.
    QPointer<PageHostWindow> self(this);
    scheduleRenderJob(new PostedJob([self]() {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) {
                return;
            }
            const int phase = self->m_hold.load(std::memory_order_acquire);
            if (phase == HoldPainted) {
                self->releaseFrameHold();
            } else if (phase == HoldArmed) {
                self->scheduleHoldRelease();
            }
        }, Qt::QueuedConnection);
    }), QQuickWindow::AfterSwapStage);
}

void PageHostWindow::retryFrameHold()
{
    if (m_hold.load(std::memory_order_acquire) == HoldIdle) {
        return;
    }
    if (m_holdRetried) {
        releaseFrameHold();
        return;
    }
    m_holdRetried = true;
    scheduleHoldRelease();
    m_holdTimer.start(kFrameHoldFallbackMs);
}

void PageHostWindow::releaseFrameHold()
{
    if (m_hold.exchange(HoldIdle, std::memory_order_acq_rel) == HoldIdle) {
        return;
    }
    m_holdTimer.stop();
    m_holdRetried = false;
    // m_appliedClip is still the 1px region, so the unclipped path clears it.
    applyPieceClip();
}

} // namespace gazer
