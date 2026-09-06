#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "ui/ScrollBar.h"

#include <QtGlobal>

namespace gazer {

namespace {

constexpr double kFollowLerp = 0.32;

const PageTarget* frontScrollTrack(const QVector<PageTarget>& targets)
{
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (localIdOf(t) != compose_detail::kOverlayScrollTrackId) {
            continue;
        }
        if (t.pageId == ComposeUi::kHistoryLiveId || t.pageId == ComposeUi::kVoicesLiveId) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace

void ComposeUi::onGaze(const GazePoint& point)
{
    if (!hasLive(kHistoryLiveId) && !hasLive(kVoicesLiveId)) {
        stopListScroll();
        return;
    }
    feedListScrollGaze(point);
}

void ComposeUi::stopListScroll()
{
    m_listScroll = ListScroll::None;
    m_listScrollEngageAtMs = -1;
    m_listScrollT = 0.0;
    m_listScrollGrace.reset();
}

void ComposeUi::feedListScrollGaze(const GazePoint& point)
{
    if (m_pages.isDwellSuspended()) {
        stopListScroll();
        return;
    }
    m_listScrollGrace.graceMs = qMax(0, m_settings.dwellGraceMs);
    if (!m_listScrollClock.isValid()) {
        m_listScrollClock.start();
    }
    const qint64 now = point.timestampMs > 0 ? point.timestampMs : m_listScrollClock.elapsed();

    auto leaveTrack = [&]() {
        if (m_listScroll == ListScroll::None && m_listScrollEngageAtMs < 0) {
            return;
        }
        if (!point.valid || m_listScrollGrace.onInvalid(now) != InvalidGazeGrace::Result::Holding) {
            stopListScroll();
        }
    };

    if (!point.valid) {
        leaveTrack();
        return;
    }

    const QTransform* xf = m_pages.window() ? &m_pages.window()->drawerXf() : nullptr;
    const PageTarget* hitT =
        PageHit::at(m_pages.targets(), point.toPointF(), m_pages.drawerScale(), {},
                    m_pages.gridPaints(), xf);
    if (hitT && hitT->interactive) {
        stopListScroll();
        return;
    }

    const PageTarget* track = frontScrollTrack(m_pages.targets());
    if (!track) {
        stopListScroll();
        return;
    }
    const QRectF cell = track->geom.contentOnScreen();
    if (cell.isEmpty()) {
        stopListScroll();
        return;
    }
    const QRectF bound = cell.adjusted(-2.0, -6.0, 2.0, 6.0);
    if (!bound.contains(point.toPointF())) {
        leaveTrack();
        return;
    }
    m_listScrollGrace.onValid();

    const ScrollBar::Spec spec = ScrollBar::parseSpec(track->caption);
    if (spec.maxOffset() <= 0) {
        stopListScroll();
        return;
    }

    const double rawT = ScrollBar::visual(cell, spec).tAtY(point.y);
    if (m_listScrollEngageAtMs < 0) {
        m_listScrollEngageAtMs = now;
        return;
    }
    if (now - m_listScrollEngageAtMs < qMax(0, m_settings.dailyScanGraceMs)) {
        return;
    }

    const ListScroll kind = track->pageId == kHistoryLiveId ? ListScroll::History
                                                            : ListScroll::Voices;
    if (m_listScroll != kind) {
        m_listScroll = kind;
        m_listScrollT = rawT;
    } else {
        m_listScrollT += (rawT - m_listScrollT) * kFollowLerp;
    }

    const int offset = qBound(0, int(qRound(m_listScrollT * double(spec.maxOffset()))),
                              spec.maxOffset());
    if (kind == ListScroll::History) {
        if (offset != m_historyPage) {
            historyGoto(offset);
        }
    } else if (offset != m_voicePage) {
        voicesGoto(offset);
    }
}

} // namespace gazer
