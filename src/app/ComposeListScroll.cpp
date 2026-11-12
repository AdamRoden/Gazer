#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "assist/GazeTrack.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/BoardPaint.h"
#include "ui/PageHostWindow.h"
#include "ui/PhraseLayout.h"
#include "ui/ScrollBar.h"

#include <QFont>
#include <QtGlobal>

namespace gazer {

namespace {

const PageTarget* frontScrollTrack(const QVector<PageTarget>& targets)
{
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (localIdOf(t) != compose_detail::kOverlayScrollTrackId) {
            continue;
        }
        if (t.pageId == ComposeUi::kHistoryLiveId || t.pageId == ComposeUi::kVoicesLiveId
            || t.pageId == ComposeUi::kFilesLiveId || t.pageId == ComposeUi::kItemEditLiveId
            || t.pageId == ComposeUi::kStylesLiveId || t.pageId == ComposeUi::kTagsLiveId
            || t.pageId == ComposeUi::kRailLiveId) {
            return &t;
        }
    }
    return nullptr;
}

const PageTarget* fileNameScroller(const QVector<PageTarget>& targets)
{
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (t.pageId == ComposeUi::kFilesLiveId && localIdOf(t) == QLatin1String("file_caret")) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace

void ComposeUi::onGaze(const GazePoint& point)
{
    if (!hasLive(kHistoryLiveId) && !hasLive(kVoicesLiveId) && !hasLive(kFilesLiveId)
        && !hasLive(kItemEditLiveId) && !hasLive(kStylesLiveId) && !hasLive(kTagsLiveId)
        && !hasLive(kRailLiveId)) {
        stopListScroll();
        return;
    }
    feedListScrollGaze(point);
}

void ComposeUi::stopListScroll()
{
    if (m_pages.window()) {
        m_pages.window()->releaseSliderScrub(m_listScrollLock.key);
    }
    m_listScroll = ListScroll::None;
    m_listScrollLock.clear();
    m_listScrollDrag = {};
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
        if (m_listScroll == ListScroll::None && m_listScrollLock.key.isEmpty()) {
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

    if (const PageTarget* name = fileNameScroller(m_pages.targets())) {
        const QRectF cell = name->geom.contentOnScreen();
        const QRectF bound = cell.adjusted(-2.0, -6.0, 2.0, 6.0);
        if (!cell.isEmpty() && bound.contains(point.toPointF())) {
            m_listScrollGrace.onValid();
            const QFont font(BoardPaint::segoeFamily(), kFileNameFontPx, QFont::Normal);
            const QRectF pad = phrasePad(cell);
            const int caret = name->caretIndex >= 0 ? name->caretIndex : 0;
            const FileNameScroll scrolled =
                fileNameScroll(font, name->label, caret, pad, !name->caption.isEmpty());
            const QString id = sessionKey(*name);
            if (m_listScrollLock.key != id) {
                m_listScrollDrag = {};
            }
            const int grace = name->dwell.scanGrace.value_or(kTrackLockMs);
            const double progress = m_listScrollLock.arm(id, now, grace);
            const double span = qMax(scrolled.textWidth, 1.0);
            const double t = qBound(0.0, scrolled.caretAdvance / span, 1.0);
            if (m_pages.window()) {
                m_pages.window()->setSliderScrub(id, t, progress >= 1.0 ? 0.0 : progress);
            }
            if (progress < 1.0 || scrolled.viewWidth <= 1.0) {
                return;
            }
            if (m_listScroll != ListScroll::FileName) {
                m_listScroll = ListScroll::FileName;
                m_listScrollDrag = {};
            }
            const double track1 = qMax(scrolled.textWidth, scrolled.viewWidth);
            if (track1 <= 1.0) {
                return;
            }
            const double gazeDoc = (point.x - scrolled.line.left()) + scrolled.scroll;
            const double pos = GazeTrack::moveThumb(m_listScrollDrag, gazeDoc, scrolled.caretAdvance,
                                                    0.0, track1, now);
            const int next = fileNameCaretAt(font, name->label, pos);
            if (next != m_fileName.caret() || m_fileName.hasSelection()) {
                m_fileName.setCaret(next);
                rebuildFiles();
            }
            return;
        }
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

    const QString id = sessionKey(*track);
    if (m_listScrollLock.key != id) {
        m_listScrollDrag = {};
    }
    const int grace = track->dwell.scanGrace.value_or(kTrackLockMs);
    const double progress = m_listScrollLock.arm(id, now, grace);
    if (m_pages.window()) {
        m_pages.window()->setSliderScrub(id, spec.t(), progress >= 1.0 ? 0.0 : progress);
    }
    if (progress < 1.0) {
        return;
    }

    ListScroll kind = ListScroll::History;
    if (track->pageId == kVoicesLiveId) {
        kind = ListScroll::Voices;
    } else if (track->pageId == kFilesLiveId) {
        kind = ListScroll::Files;
    } else if (track->pageId == kItemEditLiveId) {
        kind = ListScroll::Icons;
    } else if (track->pageId == kStylesLiveId) {
        kind = ListScroll::Styles;
    } else if (track->pageId == kTagsLiveId) {
        kind = ListScroll::Tags;
    } else if (track->pageId == kRailLiveId) {
        kind = ListScroll::Rail;
    }
    if (m_listScroll != kind) {
        m_listScroll = kind;
        m_listScrollDrag = {};
    }
    const ScrollBar::Visual vis = ScrollBar::visual(cell, spec);
    const double travel0 = vis.track.top() + vis.thumb.height() * 0.5;
    const double travel1 = vis.track.bottom() - vis.thumb.height() * 0.5;
    const double usable = travel1 - travel0;
    if (usable <= 1.0) {
        return;
    }
    const double pos = GazeTrack::moveThumb(m_listScrollDrag, point.y, vis.thumb.center().y(),
                                            travel0, travel1, now);
    const double t = qBound(0.0, (pos - travel0) / usable, 1.0);
    const int offset = qBound(0, int(qRound(t * double(spec.maxOffset()))), spec.maxOffset());
    if (kind == ListScroll::History) {
        if (offset != m_historyPage) {
            historyGoto(offset);
        }
    } else if (kind == ListScroll::Voices) {
        if (offset != m_voicePage) {
            voicesGoto(offset);
        }
    } else if (kind == ListScroll::Icons) {
        if (offset != m_iconRow) {
            iconsGoto(offset);
        }
    } else if (kind == ListScroll::Styles) {
        if (offset != m_stylePage) {
            stylesGoto(offset);
        }
    } else if (kind == ListScroll::Tags) {
        if (offset != m_tagPage) {
            tagsGoto(offset);
        }
    } else if (kind == ListScroll::Rail) {
        if (offset != m_railPage) {
            railGoto(offset);
        }
    } else if (offset != m_fileOffset) {
        filesGoto(offset);
    }
}

} // namespace gazer
