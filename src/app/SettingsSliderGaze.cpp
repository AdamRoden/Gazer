#include "app/SettingsUi.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "ui/SliderTrack.h"

#include <QtGlobal>

namespace gazer {

using SettingsUiInternal::colorChannelRange;
using SettingsUiInternal::colorChannelValueText;
using SettingsUiInternal::findColorAxis;
using SettingsUiInternal::localIdOf;

bool SettingsUi::beginSliderScrub(const QString& channel)
{
    if (m_hexActive || m_numpad.active) {
        return false;
    }
    const bool opacity = m_opacity.active
                         && channel.compare(QLatin1String("opacity"), Qt::CaseInsensitive) == 0;
    if (!opacity && (!m_color.active || !findColorAxis(channel))) {
        return false;
    }
    if (m_scrub.active && m_scrub.channel == channel) {
        return true;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    m_scrub.active = true;
    m_scrub.channel = channel;
    m_scrub.itemId = QStringLiteral("track_%1").arg(channel);
    m_scrubOpacityRevert = m_opacityDraft;
    m_scrubRevert = m_colorDraft;
    m_scrubDwell.reset();
    m_scrubDwell.setDwellMs(m_settings.mouseMoveDwellMs);
    m_scrubGrace.reset();
    m_scrubGrace.graceMs = qMax(0, m_settings.dwellGraceMs);
    if (!m_scrubClock.isValid()) {
        m_scrubClock.start();
    }
    m_scrubLastSampleMs = -1;
    m_scrubDeadlineMs = m_settings.mouseMoveSelectTimeoutMs > 0
                            ? m_scrubClock.elapsed() + m_settings.mouseMoveSelectTimeoutMs
                            : -1;
    syncSliderScrubVisuals();
    notifyStatus(QStringLiteral("Look along the slider — dwell to set, timeout to cancel"));
    return true;
}

void SettingsUi::endSliderScrub(bool commit)
{
    if (!m_scrub.active) {
        return;
    }
    if (!commit) {
        if (m_opacity.active) {
            m_opacityDraft = m_scrubOpacityRevert;
        } else {
            loadColorDraft(m_scrubRevert);
        }
    }
    m_scrub.reset();
    m_scrubDeadlineMs = -1;
    m_scrubLastSampleMs = -1;
    m_scrubDwell.reset();
    if (PageHostWindow* w = m_pages.window()) {
        w->clearSliderScrub();
    }
    if (m_opacity.active) {
        refreshOpacityEditor();
    } else {
        refreshColorPicker();
    }
}

int SettingsUi::scrubShownValue() const
{
    if (m_opacity.active) {
        return m_opacityDraft;
    }
    return colorShownValue(m_scrub.channel);
}

void SettingsUi::syncSliderScrubVisuals()
{
    PageHostWindow* w = m_pages.window();
    if (!w) {
        return;
    }
    const int shown = scrubShownValue();
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const double t = (maxV > minV) ? (shown - minV) / double(maxV - minV) : 0.0;
    if (m_opacity.active) {
        w->setPreviewColor(flashOpacityPreview());
    } else if (m_color.active) {
        w->setPreviewColor(m_colorDraft);
    }
    w->setSliderScrub(m_scrub.itemId, t, colorChannelValueText(m_scrub.channel, shown),
                      m_scrubDwell.progress());
}

void SettingsUi::onGaze(const GazePoint& point)
{
    if (m_scrub.active) {
        feedSliderGaze(point);
    }
}

void SettingsUi::feedSliderGaze(const GazePoint& point)
{
    if (!m_scrub.active) {
        return;
    }
    PageHostWindow* w = m_pages.window();
    if (!w) {
        endSliderScrub(false);
        return;
    }

    const qint64 now = m_scrubClock.isValid() ? m_scrubClock.elapsed() : 0;
    if (m_scrubDeadlineMs >= 0 && now >= m_scrubDeadlineMs) {
        notifyStatus(QStringLiteral("Slider timed out — restored previous value"));
        endSliderScrub(false);
        return;
    }

    if (!point.valid) {
        if (m_scrubGrace.onInvalid() == InvalidGazeGrace::Result::Holding) {
            return;
        }
        return;
    }
    m_scrubGrace.onValid();

    const QPointF gaze = point.toPointF();
    const QTransform* xf = m_pages.window() ? &m_pages.window()->drawerXf() : nullptr;
    const PageTarget* hitT =
        PageHit::at(m_pages.targets(), gaze, m_pages.drawerScale(), {}, m_pages.gridPaints(), xf);
    const QString hit = hitT ? localIdOf(*hitT) : QString();
    const QString editId = QStringLiteral("edit_%1").arg(m_scrub.channel);
    const QString decId = QStringLiteral("dec_%1").arg(m_scrub.channel);
    const QString incId = QStringLiteral("inc_%1").arg(m_scrub.channel);
    const bool companion = hit == m_scrub.itemId || hit == editId || hit == decId || hit == incId;
    if (hitT && hitT->interactive && !companion) {
        endSliderScrub(true);
        return;
    }

    const PageTarget* track = nullptr;
    for (const PageTarget& t : m_pages.targets()) {
        if (localIdOf(t) == m_scrub.itemId) {
            track = &t;
            break;
        }
    }
    if (!track) {
        endSliderScrub(true);
        return;
    }
    const QRectF cell = track->geom.contentOnScreen();
    if (cell.isEmpty()) {
        endSliderScrub(true);
        return;
    }
    const SliderTrack::Visual geom = SliderTrack::visual(cell, /*scrubbing=*/true);
    const QRectF bound = cell.adjusted(0, -12, 0, 12);
    if (!bound.contains(gaze)) {
        return;
    }

    const double t = geom.tAtX(gaze.x());
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const int shown = minV + qRound(t * double(maxV - minV));
    if (m_opacity.active) {
        m_opacityDraft = qBound(0, shown, 100);
    } else {
        applyColorShownValue(m_scrub.channel, shown);
    }

    const double dtSec =
        m_scrubLastSampleMs < 0 ? 0.016
                                : qBound(0.004, (now - m_scrubLastSampleMs) / 1000.0, 0.08);
    m_scrubLastSampleMs = now;
    const bool done = m_scrubDwell.sample(geom.posAt(t), dtSec);
    syncSliderScrubVisuals();
    if (done) {
        notifyStatus(QStringLiteral("%1 = %2")
                         .arg(QString(m_scrub.channel).toUpper(),
                              colorChannelValueText(m_scrub.channel, shown)));
        endSliderScrub(true);
    }
}

} // namespace gazer
