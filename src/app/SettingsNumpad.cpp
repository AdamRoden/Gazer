#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"
#include "mapping/HeadPoseCurve.h"

#include "layout/PageSession.h"

#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveNumpad;

void SettingsUi::resetNumpad()
{
    unbindEditorKeyboard();
    if (!m_numpad.pageId.isEmpty()) {
        m_pages.closePage(m_numpad.pageId);
    }
    m_numpad.reset();
    m_numpadKey.clear();
    m_numpadTitle.clear();
    m_numpadHint.clear();
    m_numpadResetSeed.clear();
    m_numpadBuffer.clear();
    m_numpadReturn = NumpadReturn::Catalog;
    m_numpadColorChannel.clear();
    m_numpadArrayIndex = -1;
}

bool SettingsUi::presentNumpad(QString* error)
{
    if (presentLive(m_numpad, QLatin1String(kLiveNumpad), buildNumpadDocument(), error)) {
        bindEditorKeyboard();
        return true;
    }
    resetNumpad();
    return false;
}

PageDocument SettingsUi::buildNumpadDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveNumpad);
    doc.name = m_numpadTitle;
    initGrid(doc, 4, 5, 560, 700, 10, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = starTracks({0.7, 1.0, 4.0, 1.0, 1.0});
    const EditorSwatch sw = editorSwatch();
    const QColor keyBg = m_settings.resolvedTheme().bgAt(90);
    PageCell title = cell(QStringLiteral("title"), m_numpadTitle, 0, 0, {}, QColor(), 4,
                          QStringLiteral("label"));
    title.textStyle = QStringLiteral("title");
    grid.cells.push_back(title);
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 1, 0,
                              QStringLiteral("settings.numpad.clear"), sw.warn));
    PageCell display =
        cell(QStringLiteral("display"),
             m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer, 1, 1, {}, sw.value, 2,
             QStringLiteral("value"));
    grid.cells.push_back(display);
    grid.cells.push_back(cell(QStringLiteral("back"), QStringLiteral("⌫"), 1, 3,
                              QStringLiteral("settings.numpad.backspace"), sw.warn));

    PageGrid keys;
    keys.id = QStringLiteral("keys");
    keys.nested = true;
    keys.row = 2;
    keys.col = 0;
    keys.colSpan = 4;
    keys.rows = 4;
    keys.columns = 3;
    keys.gapPx = 10;
    auto key = [&](const QString& id, const QString& label, int row, int col, const QString& cmd) {
        PageCell c = cell(id, label, row, col, cmd, keyBg);
        c.style.background.token = QStringLiteral("bg90");
        keys.cells.push_back(std::move(c));
    };
    key(QStringLiteral("d7"), QStringLiteral("7"), 0, 0, QStringLiteral("settings.numpad.digit.7"));
    key(QStringLiteral("d8"), QStringLiteral("8"), 0, 1, QStringLiteral("settings.numpad.digit.8"));
    key(QStringLiteral("d9"), QStringLiteral("9"), 0, 2, QStringLiteral("settings.numpad.digit.9"));
    key(QStringLiteral("d4"), QStringLiteral("4"), 1, 0, QStringLiteral("settings.numpad.digit.4"));
    key(QStringLiteral("d5"), QStringLiteral("5"), 1, 1, QStringLiteral("settings.numpad.digit.5"));
    key(QStringLiteral("d6"), QStringLiteral("6"), 1, 2, QStringLiteral("settings.numpad.digit.6"));
    key(QStringLiteral("d1"), QStringLiteral("1"), 2, 0, QStringLiteral("settings.numpad.digit.1"));
    key(QStringLiteral("d2"), QStringLiteral("2"), 2, 1, QStringLiteral("settings.numpad.digit.2"));
    key(QStringLiteral("d3"), QStringLiteral("3"), 2, 2, QStringLiteral("settings.numpad.digit.3"));
    key(QStringLiteral("minus"), QStringLiteral("−"), 3, 0, QStringLiteral("settings.numpad.minus"));
    key(QStringLiteral("d0"), QStringLiteral("0"), 3, 1, QStringLiteral("settings.numpad.digit.0"));
    key(QStringLiteral("period"), QStringLiteral("."), 3, 2, QStringLiteral("settings.numpad.period"));
    grid.subGrids.push_back(std::move(keys));

    grid.cells.push_back(cell(QStringLiteral("copy"), QStringLiteral("Copy"), 3, 0,
                              QStringLiteral("settings.numpad.copy"), sw.nudge, 2));
    grid.cells.push_back(cell(QStringLiteral("paste"), QStringLiteral("Paste"), 3, 2,
                              QStringLiteral("settings.numpad.paste"), sw.nudge, 2));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 4, 0,
                              QStringLiteral("settings.numpad.save"), sw.save, 2));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 4, 2,
                              QStringLiteral("settings.numpad.cancel"), sw.cancel, 2));
    return doc;
}

bool SettingsUi::openNumericEditor(const QString& settingKey, QString* error)
{
    if (AppSettings::isSequenceKey(settingKey)) {
        return openArrayEditor(settingKey, error);
    }
    if (!AppSettings::isNumericKey(settingKey)) {
        if (error) {
            *error = QStringLiteral("Not a numeric setting: %1").arg(settingKey);
        }
        return false;
    }
    m_numpadKey = settingKey;
    m_numpadTitle = AppSettings::settingTitle(settingKey);
    m_numpadHint = AppSettings::settingDescription(settingKey);
    m_numpadResetSeed = m_settings.numericBufferSeed(settingKey);
    m_numpadBuffer = m_numpadResetSeed;
    m_numpadReturn = NumpadReturn::Catalog;
    m_numpadArrayIndex = -1;
    m_numpadColorChannel.clear();

    if (!presentNumpad(error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Edit %1").arg(AppSettings::settingTitle(settingKey)));
    return true;
}

void SettingsUi::refreshNumpadDisplay()
{
    if (!m_numpad.active) {
        return;
    }
    QString err;
    (void)presentLive(m_numpad, QLatin1String(kLiveNumpad), buildNumpadDocument(), &err);
}

void SettingsUi::numpadAppend(const QString& ch)
{
    if (!m_numpad.active) {
        return;
    }
    const bool sequenceMode = AppSettings::isSequenceKey(m_numpadKey);
    if (ch == QLatin1String(".") && !sequenceMode && m_numpadBuffer.contains(QLatin1Char('.'))) {
        return;
    }
    if (ch == QLatin1String(",") && !sequenceMode) {
        return;
    }
    if (m_numpadBuffer == QLatin1String("0") && ch != QLatin1String(".")
        && ch != QLatin1String(",")) {
        m_numpadBuffer = ch;
    } else {
        if (m_numpadBuffer.size() >= 48) {
            return;
        }
        m_numpadBuffer += ch;
    }
    refreshNumpadDisplay();
}

void SettingsUi::numpadBackspace()
{
    if (!m_numpad.active || m_numpadBuffer.isEmpty()) {
        return;
    }
    m_numpadBuffer.chop(1);
    refreshNumpadDisplay();
}

void SettingsUi::numpadClear()
{
    if (!m_numpad.active) {
        return;
    }
    m_numpadBuffer.clear();
    refreshNumpadDisplay();
}

void SettingsUi::numpadReset()
{
    if (!m_numpad.active) {
        return;
    }
    m_numpadBuffer = m_numpadResetSeed;
    refreshNumpadDisplay();
}

void SettingsUi::numpadMinus()
{
    if (!m_numpad.active) {
        return;
    }
    if (m_numpadBuffer.startsWith(QLatin1Char('-'))) {
        m_numpadBuffer.remove(0, 1);
    } else {
        m_numpadBuffer.prepend(QLatin1Char('-'));
    }
    refreshNumpadDisplay();
}

void SettingsUi::numpadCopy()
{
    if (!m_numpad.active) {
        return;
    }
    QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    const QString text = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    clip->setText(text);
    notifyStatus(QStringLiteral("Copied %1").arg(text));
}

void SettingsUi::numpadPaste()
{
    if (!m_numpad.active) {
        return;
    }
    const QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    QString out;
    for (const QChar c : clip->text().trimmed()) {
        if (c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('.') || c == QLatin1Char(',')) {
            out += c;
        }
    }
    if (out.isEmpty()) {
        notifyStatus(QStringLiteral("Clipboard has no number"));
        return;
    }
    m_numpadBuffer = out;
    refreshNumpadDisplay();
    notifyStatus(QStringLiteral("Pasted %1").arg(out));
}

bool SettingsUi::numpadSave(QString* error)
{
    if (!m_numpad.active) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    const QString buf = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    const NumpadReturn ret = m_numpadReturn;
    const int arrayIndex = m_numpadArrayIndex;
    const QString colorCh = m_numpadColorChannel;
    const QString key = m_numpadKey;

    if (ret == NumpadReturn::Array) {
        bool ok = false;
        const int v = buf.toInt(&ok);
        if (!ok) {
            const QString msg = QStringLiteral("Enter a whole number");
            notifyStatus(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        resetNumpad();
        if (arrayIndex >= 0 && arrayIndex < m_arrayDraft.size()) {
            m_arrayDraft[arrayIndex] = qBound(0, v, 10000);
        }
        refreshArrayEditor();
        notifyStatus(QStringLiteral("Step %1 = %2 ms").arg(arrayIndex + 1).arg(v));
        return true;
    }
    if (ret == NumpadReturn::HeadPose) {
        bool ok = false;
        const double v = buf.toDouble(&ok);
        if (!ok) {
            const QString msg = QStringLiteral("Enter a number");
            notifyStatus(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        resetNumpad();
        if (HeadPoseMap* m = headPoseDraft()) {
            if (m_headPointIndex >= 0 && m_headPointIndex < m->points.size()) {
                if (m_headPointEditOut) {
                    m->points[m_headPointIndex].out = v;
                } else {
                    m->points[m_headPointIndex].in = v;
                }
                clampHeadPoseMap(*m);
                commitHeadPoseDraft(*m);
            }
        }
        refreshHeadPoseEditor();
        return true;
    }
    if (ret == NumpadReturn::Color) {
        bool ok = false;
        const int v = buf.toInt(&ok);
        if (!ok) {
            const QString msg = QStringLiteral("Enter a whole number");
            notifyStatus(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        resetNumpad();
        colorSetChannel(colorCh, v);
        refreshColorPicker();
        return true;
    }

    QString err;
    if (!m_settings.applyNumericBuffer(key, buf, &err)) {
        notifyStatus(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    const QString savedTitle = AppSettings::settingTitle(key);
    const QString savedValue = m_settings.displayValue(key);
    apply(true);

    closeLive(m_numpad);
    resetNumpad();
    notifyStatus(QStringLiteral("Saved %1 = %2").arg(savedTitle, savedValue));
    return true;
}

bool SettingsUi::numpadCancel(QString* error)
{
    if (!m_numpad.active) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    const NumpadReturn ret = m_numpadReturn;
    if (ret == NumpadReturn::Array && m_array.active) {
        closeLive(m_numpad);
        resetNumpad();
        refreshArrayEditor();
        notifyStatus(QStringLiteral("Edit cancelled"));
        return true;
    }
    if (ret == NumpadReturn::Color && m_color.active) {
        closeLive(m_numpad);
        resetNumpad();
        refreshColorPicker();
        notifyStatus(QStringLiteral("Edit cancelled"));
        return true;
    }
    if (ret == NumpadReturn::HeadPose && m_headMap.active) {
        closeLive(m_numpad);
        resetNumpad();
        refreshHeadPoseEditor();
        notifyStatus(QStringLiteral("Edit cancelled"));
        return true;
    }
    closeLive(m_numpad);
    resetNumpad();
    notifyStatus(QStringLiteral("Edit cancelled"));
    return true;
}

} // namespace gazer
