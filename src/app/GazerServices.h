#pragma once

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
#include "layout/LayoutInstanceManager.h"
#include "layout/LayoutManager.h"
#include "layout/LayoutTypes.h"
#include "mapping/MappingEngine.h"
#include "ui/MagnifierOverlay.h"

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

namespace gazer {

/// Composition root for domain services (not tray/tracker UI shell).
class GazerServices final : public QObject {
    Q_OBJECT

public:
    explicit GazerServices(QObject* parent = nullptr);

    [[nodiscard]] bool initialize(const QString& layoutsDir, const QString& mappingPath,
                                  QString* error = nullptr);

    LayoutManager& catalog() { return *m_catalog; }
    LayoutInstanceManager& instances() { return *m_instances; }
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

    AppSettings& settings() { return m_settings; }
    [[nodiscard]] const AppSettings& settings() const { return m_settings; }
    void applySettings(bool persist = true);
    [[nodiscard]] bool reloadSettings(QString* error = nullptr);
    void resetSettingsToDefaults();

signals:
    void settingsChanged();

private:
    void registerDomainCommands();
    void registerSettingsCommands();
    void notifyStatus(const QString& msg);
    void mutateAndApply(const std::function<void(AppSettings&)>& mutator, const QString& status);

    void decorateSettingsDocument(LayoutDocument& doc) const;
    void refreshOpenSettingsBoards();
    void refreshActiveIndicators();
    [[nodiscard]] bool resolveActiveState(const QString& key) const;

    [[nodiscard]] bool openNumericEditor(const QString& settingKey, QString* error = nullptr);
    void refreshNumpadDisplay();
    [[nodiscard]] LayoutDocument buildNumpadDocument() const;
    void numpadAppend(const QString& ch);
    void numpadBackspace();
    void numpadClear();
    void numpadReset();
    void numpadMinus();
    [[nodiscard]] bool numpadSave(QString* error = nullptr);
    [[nodiscard]] bool numpadCancel(QString* error = nullptr);

    [[nodiscard]] bool openColorPicker(const QString& colorKey, QString* error = nullptr);
    void closeColorPicker();

    std::unique_ptr<LayoutManager> m_catalog;
    std::unique_ptr<LayoutInstanceManager> m_instances;
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
    /// Heap-owned assist command wiring (must outlive registry handlers).
    std::unique_ptr<AssistCommandContext> m_assistCmdCtx;
    AppSettings m_settings;
    GazePoint m_lastGaze;

    // Numeric editor session (in-place on a settings secondary board).
    bool m_numpadActive = false;
    QString m_numpadInstanceId;
    QString m_numpadReturnLayoutId;
    QString m_numpadKey;
    QString m_numpadBuffer;

    bool m_colorPickerActive = false;
    QString m_colorPickerInstanceId;
    QString m_colorPickerReturnLayoutId;
    QString m_colorPickerKey;
};

} // namespace gazer
