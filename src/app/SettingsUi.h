#pragma once

#include "app/AppSettings.h"
#include "layout/LayoutTypes.h"

#include <QString>
#include <functional>

namespace gazer {

class CommandRegistry;
class LayoutInstanceManager;
class LayoutManager;

/// Settings boards: live value decoration, numeric editor, color picker, settings commands.
class SettingsUi {
public:
    using ApplyFn = std::function<void(bool persist)>;
    using NotifyFn = std::function<void(const QString&)>;
    using MutateFn = std::function<void(const std::function<void(AppSettings&)>&, const QString&)>;
    using ResetFn = std::function<void()>;

    SettingsUi(AppSettings& settings, LayoutInstanceManager& instances, LayoutManager& catalog,
               CommandRegistry& commands);

    void setApplyFn(ApplyFn fn) { m_apply = std::move(fn); }
    void setNotifyFn(NotifyFn fn) { m_notify = std::move(fn); }
    void setMutateFn(MutateFn fn) { m_mutate = std::move(fn); }
    void setResetFn(ResetFn fn) { m_reset = std::move(fn); }

    void registerCommands();
    void decorateDocument(LayoutDocument& doc) const;
    void refreshOpenBoards();

    [[nodiscard]] bool isNumpadActive() const { return m_numpadActive; }
    [[nodiscard]] QString numpadInstanceId() const { return m_numpadInstanceId; }

private:
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

    void notifyStatus(const QString& msg);
    void apply(bool persist);

    AppSettings& m_settings;
    LayoutInstanceManager& m_instances;
    LayoutManager& m_catalog;
    CommandRegistry& m_commands;
    ApplyFn m_apply;
    NotifyFn m_notify;
    MutateFn m_mutate;
    ResetFn m_reset;

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
