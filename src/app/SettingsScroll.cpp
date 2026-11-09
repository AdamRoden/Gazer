#include "app/SettingsUi.h"

#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "ui/ScrollBar.h"

#include <QtGlobal>

namespace gazer {

namespace {

constexpr int kSettingsScrollStepPx = 96;

const PageTarget* settingsScrollTrack(const QVector<PageTarget>& targets)
{
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& target = targets[i];
        if (target.role.compare(QLatin1String("scrollbar"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!target.pageId.startsWith(QLatin1String("main_settings"))) {
            continue;
        }
        return &target;
    }
    return nullptr;
}

void stampOffset(PageGrid& grid, int offset)
{
    if (grid.scroll) {
        grid.scrollOffset = offset;
    }
    for (PageGrid& sub : grid.subGrids) {
        stampOffset(sub, offset);
    }
}

} // namespace

void SettingsUi::stampSettingsScroll(PageDocument& doc)
{
    const int offset = m_settingsScroll.value(doc.id, 0);
    for (PageGrid& grid : doc.grids) {
        stampOffset(grid, offset);
    }
}

void SettingsUi::stopSettingsScroll()
{
    if (m_pages.window()) {
        m_pages.window()->releaseSliderScrub(m_settingsScrollLock.key);
    }
    m_settingsScrollLock.clear();
    m_settingsScrollDrag = {};
    m_settingsScrollGrace.reset();
}

bool SettingsUi::settingsScrollBy(int dir)
{
    if (dir == 0) {
        return false;
    }
    const PageTarget* track = settingsScrollTrack(m_pages.targets());
    if (!track) {
        return false;
    }
    const ScrollBar::Spec spec = ScrollBar::parseSpec(track->caption);
    if (spec.maxOffset() <= 0) {
        return false;
    }
    const int step = kSettingsScrollStepPx * (dir < 0 ? -1 : 1);
    const int next = qBound(0, spec.offset + step, spec.maxOffset());
    if (next == spec.offset) {
        return false;
    }
    m_settingsScroll.insert(track->pageId, next);
    m_pages.refreshDecorated();
    return true;
}

void SettingsUi::feedSettingsScroll(const GazePoint& point)
{
    if (m_pages.isDwellSuspended()) {
        stopSettingsScroll();
        return;
    }
    m_settingsScrollGrace.graceMs = qMax(0, m_settings.dwellGraceMs);
    if (!m_settingsScrollClock.isValid()) {
        m_settingsScrollClock.start();
    }
    const qint64 now = point.timestampMs > 0 ? point.timestampMs : m_settingsScrollClock.elapsed();
    auto leave = [&]() {
        if (m_settingsScrollLock.key.isEmpty()) {
            return;
        }
        if (!point.valid || m_settingsScrollGrace.onInvalid(now) != InvalidGazeGrace::Result::Holding) {
            stopSettingsScroll();
        }
    };

    if (!point.valid) {
        leave();
        return;
    }

    const QTransform* xf = m_pages.window() ? &m_pages.window()->drawerXf() : nullptr;
    const PageTarget* hit =
        PageHit::at(m_pages.targets(), point.toPointF(), m_pages.drawerScale(), {},
                    m_pages.gridPaints(), xf);
    if (hit && hit->interactive) {
        stopSettingsScroll();
        return;
    }

    const PageTarget* track = settingsScrollTrack(m_pages.targets());
    if (!track) {
        stopSettingsScroll();
        return;
    }
    const QRectF cell = track->geom.contentOnScreen();
    if (cell.isEmpty() || !cell.adjusted(-2.0, -6.0, 2.0, 6.0).contains(point.toPointF())) {
        leave();
        return;
    }
    m_settingsScrollGrace.onValid();

    const ScrollBar::Spec spec = ScrollBar::parseSpec(track->caption);
    if (spec.maxOffset() <= 0) {
        stopSettingsScroll();
        return;
    }

    const QString id = sessionKey(*track);
    if (m_settingsScrollLock.key != id) {
        m_settingsScrollDrag = {};
    }
    const int grace = track->dwell.scanGrace.value_or(kTrackLockMs);
    const double progress = m_settingsScrollLock.arm(id, now, grace);
    if (m_pages.window()) {
        m_pages.window()->setSliderScrub(id, spec.t(), progress >= 1.0 ? 0.0 : progress);
    }
    if (progress < 1.0) {
        return;
    }

    const ScrollBar::Visual vis = ScrollBar::visual(cell, spec);
    const double travel0 = vis.track.top() + vis.thumb.height() * 0.5;
    const double travel1 = vis.track.bottom() - vis.thumb.height() * 0.5;
    if (travel1 - travel0 <= 1.0) {
        return;
    }
    const double pos = GazeTrack::moveThumb(m_settingsScrollDrag, point.y, vis.thumb.center().y(),
                                            travel0, travel1, now);
    const double t = qBound(0.0, (pos - travel0) / (travel1 - travel0), 1.0);
    const int offset = qBound(0, qRound(t * double(spec.maxOffset())), spec.maxOffset());
    if (offset == spec.offset) {
        return;
    }
    m_settingsScroll.insert(track->pageId, offset);
    m_pages.refreshDecorated();
}

} // namespace gazer
