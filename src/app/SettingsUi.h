#pragma once

#include "app/AppSettings.h"
#include "assist/LookToMap.h"
#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "layout/PageTypes.h"

#include <QColor>
#include <QHash>
#include <QMetaObject>
#include <QPoint>
#include <QString>
#include <QVector>
#include <functional>

namespace gazer {

class CommandRegistry;
class ElevenClient;
class HeadPoseMapper;
class LookToMaps;
class MouseDwellMove;
class PageSession;
class SpeechSecrets;

/// Settings boards: live value decoration, numeric editor, color picker, settings commands.
/// Implementations: SettingsUi.cpp (shared), SettingsNumpad, SettingsArrayEditor,
/// SettingsColorPicker, SettingsHexEditor, SettingsSpeechKey, SettingsSliderGaze,
/// SettingsCommands, SettingsHeadPose.
class SettingsUi {
public:
    using ApplyFn = std::function<void(bool persist)>;
    using NotifyFn = std::function<void(const QString&)>;
    using MutateFn = std::function<void(const std::function<void(AppSettings&)>&, const QString&)>;
    using ResetFn = std::function<void()>;

    SettingsUi(AppSettings& settings, CommandRegistry& commands, PageSession& pages,
               SpeechSecrets& secrets, ElevenClient& eleven);

    void setApplyFn(ApplyFn fn) { m_apply = std::move(fn); }
    void setNotifyFn(NotifyFn fn) { m_notify = std::move(fn); }
    void setMutateFn(MutateFn fn) { m_mutate = std::move(fn); }
    void setResetFn(ResetFn fn) { m_reset = std::move(fn); }
    void setMouseDwellMove(MouseDwellMove* move) { m_mouseDwell = move; }
    void setHeadPoseMapper(HeadPoseMapper* mapper) { m_headPose = mapper; }
    void setLookToMaps(LookToMaps* maps) { m_lookTo = maps; }
    [[nodiscard]] LookToDest lookToEditorDest() const { return m_lookToDest; }
    [[nodiscard]] bool lookToEditorOpen() const { return m_lookToMap.active; }
    [[nodiscard]] bool lookToPreviewOn() const { return m_lookToMap.active && m_lookToPreview; }
    [[nodiscard]] HeadPoseAxis headChartAxis() const { return m_headChartAxis; }
    [[nodiscard]] QString headMapSourceId() const;
    [[nodiscard]] QString headMapDestId() const;
    [[nodiscard]] bool headMapEnabled() const;
    void syncHeadPosePaint();

    void registerCommands();
    void decoratePage(PageDocument& doc);
    /// Store the validated key, or surface the HTTP/DPAPI error. Returns true if stored.
    [[nodiscard]] bool onSpeechKeyValidated(bool ok, const QString& error);
    /// Head-pose curve gaze-scrub while that editor is open.
    void onGaze(const GazePoint& point);
    void onColorAimMoved(const QPoint& pos);
    void cancelEyedropper();
    [[nodiscard]] QString colorPickerKey() const { return m_colorPickerKey; }

    [[nodiscard]] bool isNumpadActive() const { return m_numpad.active; }

    static constexpr const char* kColorKeys[] = {
        "progressColor",      "progressFillColor",   "hoverColor",          "flashColor",
        "comboInnerColor",    "comboOuterColor",     "customPrimaryColor",  "customSecondaryColor"};

    [[nodiscard]] bool themeAssignPrimary() const { return m_themeAssignPrimary; }

    struct EditorSwatch {
        QColor key;
        QColor save;
        QColor cancel;
        QColor nudge;
        QColor warn;
        QColor add;
        QColor value;
        QColor edit;
    };

private:
    [[nodiscard]] EditorSwatch editorSwatch() const;
    [[nodiscard]] bool openNumericEditor(const QString& settingKey, QString* error = nullptr);
    void refreshNumpadDisplay();
    [[nodiscard]] PageDocument buildNumpadDocument() const;
    void numpadAppend(const QString& ch);
    void numpadBackspace();
    void numpadClear();
    void numpadReset();
    void numpadMinus();
    void numpadCopy();
    void numpadPaste();
    [[nodiscard]] bool numpadSave(QString* error = nullptr);
    [[nodiscard]] bool numpadCancel(QString* error = nullptr);
    void resetNumpad();
    [[nodiscard]] bool presentNumpad(QString* error);

    [[nodiscard]] bool openArrayEditor(const QString& settingKey, QString* error = nullptr);
    void refreshArrayEditor();
    [[nodiscard]] PageDocument buildArrayDocument() const;
    void arrayNudge(int index, int dir);
    void arrayNudgeAll(int dir);
    void arrayRemove(int index);
    void arrayAdd();
    void arrayReset();
    [[nodiscard]] bool arraySave(QString* error = nullptr);
    [[nodiscard]] bool arrayCancel(QString* error = nullptr);
    [[nodiscard]] bool arrayEditIndex(int index, QString* error = nullptr);

    [[nodiscard]] bool openColorPicker(const QString& colorKey, QString* error = nullptr);
    void refreshColorPicker();
    [[nodiscard]] PageDocument buildGenericColorDocument() const;
    void closeColorPicker();
    [[nodiscard]] bool isInlineThemeEditor() const;
    bool ensureInlineThemeEditor();
    void stopInlineThemeEditor();
    void persistThemeDraft(bool persist);
    void colorNudge(const QString& channel, int dir);
    void colorNudgeField(int ds, int dv);
    void colorSetChannel(const QString& channel, int value);
    void loadColorDraft(const QColor& c);
    void applyHsv(int h, int s, int v, int alpha);
    void colorApplyPalette(int index);
    [[nodiscard]] bool beginColorPick();
    [[nodiscard]] bool beginEyedropper();
    void restoreEyedropHost();
    void sampleScreenColor(const QPoint& pos);
    void applyPickAt(const QPoint& pos);
    [[nodiscard]] int colorShownValue(const QString& channel) const;
    bool applyColorShownValue(const QString& channel, int value);
    void themeSetAssignPrimary(bool primary);
    void themePickShade(int family, int index);
    [[nodiscard]] QColor liveThemeSource() const;
    [[nodiscard]] QString activeThemeColorKey() const;
    [[nodiscard]] QColor colorForThemeKey(const QString& key) const;
    void loadActiveThemeColor();
    void storeDraftPending();
    void refreshHexEditor();
    [[nodiscard]] bool colorSave(QString* error = nullptr);
    [[nodiscard]] bool colorEditChannel(const QString& channel, QString* error = nullptr);
    [[nodiscard]] bool openHexEditor(QString* error = nullptr);
    [[nodiscard]] PageDocument buildHexDocument() const;
    void hexAppend(QChar ch);
    void hexBackspace();
    void hexCopy();
    void hexPaste();
    [[nodiscard]] bool hexSave(QString* error = nullptr);
    [[nodiscard]] bool hexCancel(QString* error = nullptr);

    void decorateHeadPosePage(PageDocument& doc);
    void fillHeadPoseMaps(PageDocument& doc);
    HeadPoseMap* mapForChartAxis();
    const HeadPoseMap* mapForChartAxis() const;
    void setHeadChartAxis(HeadPoseAxis axis);
    void registerHeadPoseCommands();
    [[nodiscard]] bool headPoseRecenter(QString* error);
    [[nodiscard]] bool headPoseAddMap(QString* error);
    [[nodiscard]] bool openHeadPoseEditor(const QString& mapId, QString* error = nullptr);
    void refreshHeadPoseEditor();
    [[nodiscard]] PageDocument buildHeadPoseEditor() const;
    HeadPoseMap* headPoseDraft();
    const HeadPoseMap* headPoseDraft() const;
    void commitHeadPoseDraft(const HeadPoseMap& m, bool persist = true);
    [[nodiscard]] bool openHeadPoseCommandList(QString* error = nullptr);
    void refreshHeadPoseCommandList();
    [[nodiscard]] PageDocument buildHeadPoseCommandList() const;
    void beginCurveScrub();
    void endCurveScrub();
    void feedCurveGaze(const GazePoint& point);
    QStringList headPoseCommandCatalog() const;

    void registerLookToCommands();
    [[nodiscard]] bool openLookToEditor(LookToDest dest, QString* error = nullptr);
    void refreshLookToEditor();
    [[nodiscard]] PageDocument buildLookToEditor() const;
    void closeLookToEditor();
    LookToMapSettings* lookToDraft();
    const LookToMapSettings* lookToDraft() const;
    void commitLookToDraft(const LookToMapSettings& c, bool persist = true);
    void lookToNudge(LookToRing ring, int dir);
    void lookToNudgeSpeed(int dir);
    void lookToNudgeAccel(int dir);
    void lookToNudgeCenterDwell(int dir);
    [[nodiscard]] bool lookToEditField(const QString& field, QString* error = nullptr);
    void applyLookToNumpad(double v);
    void syncLookToPreview();

    [[nodiscard]] bool openSpeechKeyBoard(QString* error = nullptr);
    void refreshSpeechKeyBoard();
    [[nodiscard]] PageDocument buildSpeechKeyDocument() const;
    void speechKeyPaste();
    void speechKeyClear();
    [[nodiscard]] bool speechKeySave(QString* error = nullptr);
    void speechKeyCancel();
    [[nodiscard]] bool clearSpeechKey(QString* error = nullptr);

    void applyPreviewColor();

    struct LiveBoard {
        bool active = false;
        QString pageId;
        void reset()
        {
            active = false;
            pageId.clear();
        }
    };

    [[nodiscard]] bool presentLive(LiveBoard& board, const QString& id, PageDocument doc,
                                   QString* error);
    void closeLive(LiveBoard& board);

    void notifyStatus(const QString& msg);
    void apply(bool persist);
    void bindEditorKeyboard();
    void unbindEditorKeyboard();
    void handleEditorKey(int key, const QString& text);

    AppSettings& m_settings;
    CommandRegistry& m_commands;
    PageSession& m_pages;
    SpeechSecrets& m_secrets;
    ElevenClient& m_eleven;
    ApplyFn m_apply;
    NotifyFn m_notify;
    MutateFn m_mutate;
    ResetFn m_reset;
    MouseDwellMove* m_mouseDwell = nullptr;
    HeadPoseMapper* m_headPose = nullptr;
    LookToMaps* m_lookTo = nullptr;
    HeadPoseAxis m_headChartAxis = HeadPoseAxis::Yaw;
    LiveBoard m_lookToMap;
    LookToDest m_lookToDest = LookToDest::Scroll;
    LookToRing m_lookToRing = LookToRing::Deadzone;
    bool m_lookToPreview = false;
    QString m_lookToNumpadField;

    LiveBoard m_headMap;
    QString m_headMapId;
    int m_headPointIndex = 0;
    bool m_headPointEditOut = false;
    LiveBoard m_headCmd;
    int m_headCmdPage = 0;
    bool m_curveScrub = false;
    GazeDwellTracker m_curveDwell;
    InvalidGazeGrace m_curveLeaveGrace;
    qint64 m_curveScrubLastMs = -1;

    LiveBoard m_numpad;
    QString m_numpadKey;
    QString m_numpadTitle;
    QString m_numpadHint;
    QString m_numpadResetSeed;
    QString m_numpadBuffer;
    enum class NumpadReturn { Catalog, Array, Color, HeadPose, LookTo };
    NumpadReturn m_numpadReturn = NumpadReturn::Catalog;
    QString m_numpadColorChannel;
    int m_numpadArrayIndex = -1;

    LiveBoard m_array;
    QString m_arrayKey;
    QVector<int> m_arrayDraft;

    LiveBoard m_color;
    bool m_themeAssignPrimary = true;
    QString m_colorPickerKey;
    QHash<QString, QColor> m_colorPending;
    QColor m_colorDraft;
    int m_colorA = 255;
    bool m_eyedropActive = false;
    bool m_eyedropHostHidden = false;

    bool m_hexActive = false;
    QString m_hexBuffer;
    LiveBoard m_key;
    bool m_keyChecking = false;
    QString m_keyBuffer;
    QMetaObject::Connection m_editorKeyConn;
};

} // namespace gazer
