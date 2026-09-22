#pragma once

#include "assist/LookToMap.h"
#include "assist/LookToScroll.h"
#include "core/GazePoint.h"
#include "ui/Theme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QVector>
#include <memory>

namespace gazer {

class InputService;
class LookToOverlay;
struct LookToMapSettings;

/// Four independent analog gaze maps (scroll, mouse, left stick, right stick).
class LookToMaps final : public QObject {
    Q_OBJECT

public:
    explicit LookToMaps(QObject* parent = nullptr);
    ~LookToMaps() override;

    [[nodiscard]] LookToScroll& map(LookToDest dest);
    [[nodiscard]] const LookToScroll& map(LookToDest dest) const;
    [[nodiscard]] LookToScroll& scroll() { return map(LookToDest::Scroll); }

    void setInput(InputService* input);
    void setNotifyFn(LookToScroll::NotifyFn fn);
    void setAccent(const QColor& c);
    void setTheme(const ThemeColors& theme);
    void setPieRadii(int innerPx, int sharedPx, int outerPx);
    void setAnnulusColors(const QColor& inner, const QColor& outer);
    void setScanGraceMs(int ms);
    void setDwellGraceMs(int ms);
    void setDwellSequence(const QVector<int>& ms);
    void applyConfig(LookToDest dest, const LookToMapSettings& cfg);

    void setPreview(LookToDest dest, bool on);
    void clearPreview();
    void setHighlightRing(LookToDest dest, LookToRing ring);

    void beginPlace(LookToDest dest);
    void onPlaced(QPoint pos);
    void cancelPlace();
    [[nodiscard]] bool isPlacing() const { return m_placing; }
    [[nodiscard]] LookToDest placingDest() const { return m_placingDest; }

    void disableAll();
    void setOutputPaused(bool on);
    [[nodiscard]] bool outputPaused() const { return m_outputPaused; }
    [[nodiscard]] bool anyEnabled() const;
    [[nodiscard]] bool containsGaze(const GazePoint& point) const;
    [[nodiscard]] bool anyPieOpen() const;

    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(LookToDest dest, bool enabled);
    void outputSuspendedChanged(LookToDest dest, bool suspended);
    void maxSpeedChanged(LookToDest dest, double speed);
    void axisModeChanged(LookToDest dest, LtsScrollMode mode);
    void placeOriginRequested(LookToDest dest);

private:
    void connectMap(LookToDest dest);
    void updatePinCursor();
    void updatePreviewHud();
    template <typename Fn>
    void forEachMap(Fn&& fn)
    {
        for (int i = 0; i < kLookToDestCount; ++i) {
            fn(*m_maps[i]);
        }
    }

    std::unique_ptr<LookToScroll> m_maps[kLookToDestCount];
    std::unique_ptr<LookToOverlay> m_previewHud;
    bool m_placing = false;
    LookToDest m_placingDest = LookToDest::Scroll;
    bool m_preview = false;
    LookToDest m_previewDest = LookToDest::Scroll;
    LookToRing m_highlight = LookToRing::None;
    GazePoint m_lastGaze;
    QColor m_accent;
    bool m_outputPaused = false;
    QElapsedTimer m_idlePause;
};

} // namespace gazer
