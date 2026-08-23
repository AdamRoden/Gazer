#pragma once

#include "app/AppSettings.h"
#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "layout/PageTypes.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QMetaObject>
#include <QString>
#include <QVector>
#include <functional>

namespace gazer {

class CommandRegistry;
class PageSession;

/// Settings boards: live value decoration, numeric editor, color picker, settings commands.
class SettingsUi {
public:
    using ApplyFn = std::function<void(bool persist)>;
    using NotifyFn = std::function<void(const QString&)>;
    using MutateFn = std::function<void(const std::function<void(AppSettings&)>&, const QString&)>;
    using ResetFn = std::function<void()>;

    SettingsUi(AppSettings& settings, CommandRegistry& commands, PageSession& pages);

    void setApplyFn(ApplyFn fn) { m_apply = std::move(fn); }
    void setNotifyFn(NotifyFn fn) { m_notify = std::move(fn); }
    void setMutateFn(MutateFn fn) { m_mutate = std::move(fn); }
    void setResetFn(ResetFn fn) { m_reset = std::move(fn); }

    void registerCommands();
    void decoratePage(PageDocument& doc) const;
    /// Gaze-follow color slider after the track is activated.
    void onGaze(const GazePoint& point);
    [[nodiscard]] bool isSliderScrubbing() const { return m_scrub.active; }
    [[nodiscard]] QString colorPickerKey() const { return m_colorPickerKey; }

    [[nodiscard]] bool isNumpadActive() const { return m_numpad.active; }

    static constexpr const char* kColorKeys[] = {
        "progressColor",      "progressFillColor",   "progressBorderColor", "flashColor",
        "customBgColor",      "customPrimaryColor",  "customSecondaryColor", "customTertiaryColor",
        "customSurfaceColor", "customTextColor",     "customDangerColor"};

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

    [[nodiscard]] bool openFlashForeground(QString* error = nullptr);
    [[nodiscard]] bool openFlashCustom(QString* error = nullptr);
    [[nodiscard]] bool openOpacityEditor(QString* error = nullptr);
    void refreshOpacityEditor();
    [[nodiscard]] PageDocument buildOpacityDocument() const;
    void closeOpacityEditor();
    void opacityNudge(int dir);
    [[nodiscard]] bool opacitySave(QString* error = nullptr);
    [[nodiscard]] bool openColorPicker(const QString& colorKey, QString* error = nullptr);
    [[nodiscard]] bool selectColorTarget(const QString& colorKey, QString* error = nullptr);
    void refreshColorPicker();
    [[nodiscard]] PageDocument buildColorDocument() const;
    void closeColorPicker();
    void colorNudge(const QString& channel, int dir);
    void colorSetChannel(const QString& channel, int value);
    void colorUseSaved(const QString& savedKey);
    void colorSyncFromHsv();
    void colorSyncFromRgb();
    void loadColorDraft(const QColor& c);
    [[nodiscard]] int colorShownValue(const QString& channel) const;
    bool applyColorShownValue(const QString& channel, int value);
    bool beginSliderScrub(const QString& channel);
    void endSliderScrub(bool commit);
    void feedSliderGaze(const GazePoint& point);
    void syncSliderScrubVisuals();
    [[nodiscard]] int scrubShownValue() const;
    [[nodiscard]] ThemePalette draftThemePalette() const;
    [[nodiscard]] AppSettings draftThemeSettings() const;
    [[nodiscard]] QColor suggestedDraftColor(const QString& colorKey) const;
    void applySuggestedColor(const QString& colorKey);
    void refreshHexEditor();
    [[nodiscard]] bool colorSave(QString* error = nullptr);
    [[nodiscard]] bool colorEditChannel(const QString& channel, QString* error = nullptr);
    [[nodiscard]] bool openHexEditor(QString* error = nullptr);
    [[nodiscard]] PageDocument buildHexDocument() const;
    void hexAppend(QChar ch);
    void hexBackspace();
    [[nodiscard]] bool hexSave(QString* error = nullptr);
    [[nodiscard]] bool hexCancel(QString* error = nullptr);

    void applyPreviewColor();
    [[nodiscard]] QColor flashOpacityPreview() const;

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
    ApplyFn m_apply;
    NotifyFn m_notify;
    MutateFn m_mutate;
    ResetFn m_reset;

    LiveBoard m_numpad;
    QString m_numpadKey;
    QString m_numpadTitle;
    QString m_numpadHint;
    QString m_numpadResetSeed;
    QString m_numpadBuffer;
    enum class NumpadReturn { Catalog, Array, Color };
    NumpadReturn m_numpadReturn = NumpadReturn::Catalog;
    QString m_numpadColorChannel;
    int m_numpadArrayIndex = -1;

    LiveBoard m_array;
    QString m_arrayKey;
    QVector<int> m_arrayDraft;

    LiveBoard m_opacity;
    int m_opacityDraft = 60;
    int m_opacityRevert = 60;
    bool m_opacitySetMode = false;

    LiveBoard m_color;
    bool m_flashCustomSetMode = false;
    QString m_colorPickerKey;
    QHash<QString, QColor> m_colorPending;
    QColor m_colorDraft;
    int m_colorH = 180;
    int m_colorS = 255;
    int m_colorV = 255;
    int m_colorR = 0;
    int m_colorG = 220;
    int m_colorB = 255;
    int m_colorA = 255;

    bool m_hexActive = false;
    QString m_hexBuffer;
    QMetaObject::Connection m_editorKeyConn;

    struct SliderScrub {
        bool active = false;
        QString channel;
        QString itemId;
        void reset()
        {
            active = false;
            channel.clear();
            itemId.clear();
        }
    };
    SliderScrub m_scrub;
    QColor m_scrubRevert;
    int m_scrubOpacityRevert = 60;
    GazeDwellTracker m_scrubDwell;
    InvalidGazeGrace m_scrubGrace;
    QElapsedTimer m_scrubClock;
    qint64 m_scrubDeadlineMs = -1;
    qint64 m_scrubLastSampleMs = -1;
};

} // namespace gazer
