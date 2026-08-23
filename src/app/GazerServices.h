#pragma once

#include "app/ActiveStateResolver.h"
#include "app/AppSettings.h"
#include "app/CommandRegistry.h"
#include "core/GazePoint.h"
#include "assist/ActionLoopService.h"
#include "assist/AssistCommands.h"
#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "assist/PhraseService.h"
#include "assist/ScriptHost.h"
#include "assist/TtsService.h"
#include "input/InputService.h"
#include "layout/PageCatalog.h"
#include "layout/PageTypes.h"
#include "mapping/MappingEngine.h"
#include "ui/MagnifierOverlay.h"

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace gazer {

class SettingsUi;
class PageSession;

/// Composition root for domain services (not tray/tracker UI shell).
class GazerServices final : public QObject {
    Q_OBJECT

public:
    explicit GazerServices(QObject* parent = nullptr);
    ~GazerServices() override;

    [[nodiscard]] bool initialize(const QString& layoutsDir, const QString& mappingPath,
                                  QString* error = nullptr);

    using ActionDispatchFn =
        std::function<void(const QVector<PageAction>& actions, const QString& pageId,
                           const QString& targetId)>;
    void bindActionDispatch(ActionDispatchFn dispatch);

    PageCatalog& catalog() { return *m_catalog; }
    PageSession& pages() { return *m_pages; }
    InputService& input() { return *m_input; }
    MappingEngine& mapping() { return *m_mapping; }
    TtsService& tts() { return *m_tts; }
    PhraseService& phrases() { return *m_phrases; }
    CommandRegistry& commands() { return *m_commands; }
    ScriptHost& scripts() { return *m_scripts; }
    LookToScroll& lookToScroll() { return *m_lookToScroll; }
    MagnifierOverlay& magnifier() { return *m_magnifier; }
    MouseDwellMove& mouseDwellMove() { return *m_mouseDwellMove; }
    MouseAssistState& mouseAssist() { return *m_mouseAssist; }
    GazeReticle& gazeReticle() { return *m_gazeReticle; }
    GazeMouseFollow& gazeMouseFollow() { return *m_gazeMouseFollow; }
    AssistSession& assistSession() { return *m_assistSession; }
    ActionLoopService& actionLoops() { return *m_actionLoops; }

    /// Last valid gaze sample (for mouseMoveToGaze command / loops).
    void setLastGaze(const GazePoint& g) { m_lastGaze = g; }
    [[nodiscard]] GazePoint lastGaze() const { return m_lastGaze; }

    SettingsUi& settingsUi() { return *m_settingsUi; }
    AppSettings& settings() { return m_settings; }
    [[nodiscard]] const AppSettings& settings() const { return m_settings; }
    void applySettings(bool persist = true);
    [[nodiscard]] bool reloadSettings(QString* error = nullptr);
    void resetSettingsToDefaults();

    void setDwellSuspended(bool on);
    void toggleDwellSuspended();
    [[nodiscard]] bool isDwellSuspended() const;

signals:
    void settingsChanged();

private:
    void registerDomainCommands();
    void notifyStatus(const QString& msg);
    void refreshMouseAmountLabels();
    void mutateAndApply(const std::function<void(AppSettings&)>& mutator, const QString& status);
    void refreshActiveIndicators();
    [[nodiscard]] ActiveStateContext activeStateContext() const;

    std::unique_ptr<PageCatalog> m_catalog;
    std::unique_ptr<PageSession> m_pages;
    std::unique_ptr<InputService> m_input;
    std::unique_ptr<MappingEngine> m_mapping;
    std::unique_ptr<TtsService> m_tts;
    std::unique_ptr<PhraseService> m_phrases;
    std::unique_ptr<CommandRegistry> m_commands;
    std::unique_ptr<ScriptHost> m_scripts;
    std::unique_ptr<LookToScroll> m_lookToScroll;
    std::unique_ptr<MagnifierOverlay> m_magnifier;
    std::unique_ptr<MouseDwellMove> m_mouseDwellMove;
    std::unique_ptr<MouseAssistState> m_mouseAssist;
    std::unique_ptr<GazeReticle> m_gazeReticle;
    std::unique_ptr<GazeMouseFollow> m_gazeMouseFollow;
    std::unique_ptr<AssistSession> m_assistSession;
    std::unique_ptr<ActionLoopService> m_actionLoops;
    std::unique_ptr<AssistCommandContext> m_assistCmdCtx;
    std::unique_ptr<SettingsUi> m_settingsUi;
    AppSettings m_settings;
    GazePoint m_lastGaze;
};

} // namespace gazer
