#pragma once

#include "app/ActiveStateResolver.h"
#include "app/AppSettings.h"
#include "layout/PageTypes.h"

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace gazer {

class ActionLoopService;
class AhkLauncher;
class AssistSession;
struct AssistCommandContext;
class ClipPlayer;
class ComboMouse;
class CommandRegistry;
class ComposeUi;
class GazeMouseFollow;
class GazeReticle;
class HeadPoseMapper;
class InputService;
class KeyStateManager;
class LookToScroll;
class MagnifierOverlay;
class MappingEngine;
class MouseAssistState;
class MouseDwellMove;
class PageCatalog;
class PageSession;
class ScriptHost;
class SoundboardStore;
class SettingsUi;
class ElevenClient;
class SpeechEngine;
class SpeechHistory;
class SpeechSecrets;
class TtsService;

/// Composition root for domain services (not tray/tracker UI shell).
/// Accessors return references; include the domain header at the call site.
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
    KeyStateManager& keyState() { return *m_keyState; }
    MappingEngine& mapping() { return *m_mapping; }
    TtsService& tts() { return *m_tts; }
    SpeechEngine& speechEngine() { return *m_speech; }
    ClipPlayer& clipPlayer() { return *m_clips; }
    CommandRegistry& commands() { return *m_commands; }
    ComposeUi& composeUi() { return *m_compose; }
    ScriptHost& scripts() { return *m_scripts; }
    AhkLauncher& ahk() { return *m_ahk; }
    LookToScroll& lookToScroll() { return *m_lookToScroll; }
    ComboMouse& comboMouse() { return *m_comboMouse; }
    MagnifierOverlay& magnifier() { return *m_magnifier; }
    MouseDwellMove& mouseDwellMove() { return *m_mouseDwellMove; }
    MouseAssistState& mouseAssist() { return *m_mouseAssist; }
    GazeReticle& gazeReticle() { return *m_gazeReticle; }
    GazeMouseFollow& gazeMouseFollow() { return *m_gazeMouseFollow; }
    HeadPoseMapper& headPoseMapper() { return *m_headPose; }
    AssistSession& assistSession() { return *m_assistSession; }
    ActionLoopService& actionLoops() { return *m_actionLoops; }

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
    std::unique_ptr<KeyStateManager> m_keyState;
    std::unique_ptr<InputService> m_input;
    std::unique_ptr<MappingEngine> m_mapping;
    std::unique_ptr<TtsService> m_tts;
    std::unique_ptr<ClipPlayer> m_clips;
    std::unique_ptr<SpeechSecrets> m_secrets;
    std::unique_ptr<ElevenClient> m_eleven;
    std::unique_ptr<SpeechEngine> m_speech;
    std::unique_ptr<CommandRegistry> m_commands;
    std::unique_ptr<SoundboardStore> m_board;
    std::unique_ptr<SpeechHistory> m_history;
    std::unique_ptr<ComposeUi> m_compose;
    std::unique_ptr<ScriptHost> m_scripts;
    std::unique_ptr<AhkLauncher> m_ahk;
    std::unique_ptr<LookToScroll> m_lookToScroll;
    std::unique_ptr<ComboMouse> m_comboMouse;
    std::unique_ptr<MagnifierOverlay> m_magnifier;
    std::unique_ptr<MouseDwellMove> m_mouseDwellMove;
    std::unique_ptr<MouseAssistState> m_mouseAssist;
    std::unique_ptr<GazeReticle> m_gazeReticle;
    std::unique_ptr<GazeMouseFollow> m_gazeMouseFollow;
    std::unique_ptr<HeadPoseMapper> m_headPose;
    std::unique_ptr<AssistSession> m_assistSession;
    std::unique_ptr<ActionLoopService> m_actionLoops;
    std::unique_ptr<AssistCommandContext> m_assistCmdCtx;
    std::unique_ptr<SettingsUi> m_settingsUi;
    AppSettings m_settings;
};

} // namespace gazer
