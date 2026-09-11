#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageSession.h"

#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::hexSeedFromColor;
using SettingsUiInternal::kLiveHex;
using SettingsUiInternal::normalizeHexDigits;
using SettingsUiInternal::parseHexDraft;

bool SettingsUi::openHexEditor(QString* error)
{
    if (!m_color.active) {
        (void)ensureInlineThemeEditor();
    }
    if (!m_color.active) {
        if (error) {
            *error = QStringLiteral("Color picker is not open");
        }
        return false;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    m_hexActive = true;
    m_hexBuffer = hexSeedFromColor(m_colorDraft);
    LiveBoard hexBoard;
    if (!presentLive(hexBoard, QLatin1String(kLiveHex), buildHexDocument(), error)) {
        m_hexActive = false;
        return false;
    }
    bindEditorKeyboard();
    notifyStatus(QStringLiteral("Enter hex color"));
    return true;
}

PageDocument SettingsUi::buildHexDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveHex);
    doc.name = QStringLiteral("Hex color");
    initGrid(doc, 4, 7, 560, 760, 10, 16, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = starTracks({1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});
    const EditorSwatch pal = editorSwatch();
    const QString shown =
        m_hexBuffer.isEmpty() ? QStringLiteral("#") : QStringLiteral("#%1").arg(m_hexBuffer);
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 0, 0,
                              QStringLiteral("settings.hex.clear"), pal.warn));
    PageCell display = cell(QStringLiteral("display"), shown, 0, 1, {}, pal.value, 2,
                            QStringLiteral("value"));
    grid.cells.push_back(display);
    grid.cells.push_back(cell(QStringLiteral("back"), QStringLiteral("⌫"), 0, 3,
                              QStringLiteral("settings.hex.backspace"), pal.warn));

    const char* keys[] = {"1", "2", "3", "A", "4", "5", "6", "B",
                          "7", "8", "9", "C", "0", "D", "E", "F"};
    for (int i = 0; i < 16; ++i) {
        const int row = 1 + i / 4;
        const int col = i % 4;
        const QString k = QLatin1String(keys[i]);
        grid.cells.push_back(cell(QStringLiteral("h_%1").arg(k), k, row, col,
                                  QStringLiteral("settings.hex.digit.%1").arg(k), pal.key));
    }
    grid.cells.push_back(cell(QStringLiteral("copy"), QStringLiteral("Copy"), 5, 0,
                              QStringLiteral("settings.hex.copy"), pal.nudge, 2));
    grid.cells.push_back(cell(QStringLiteral("paste"), QStringLiteral("Paste"), 5, 2,
                              QStringLiteral("settings.hex.paste"), pal.nudge, 2));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 6, 0,
                              QStringLiteral("settings.hex.save"), pal.save, 2));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 6, 2,
                              QStringLiteral("settings.hex.cancel"), pal.cancel, 2));
    return doc;
}

void SettingsUi::hexAppend(QChar ch)
{
    if (!m_hexActive) {
        return;
    }
    if (m_hexBuffer.size() >= 8) {
        return;
    }
    m_hexBuffer += ch.toUpper();
    refreshHexEditor();
}

void SettingsUi::hexBackspace()
{
    if (!m_hexActive || m_hexBuffer.isEmpty()) {
        return;
    }
    m_hexBuffer.chop(1);
    refreshHexEditor();
}

void SettingsUi::hexCopy()
{
    if (!m_hexActive) {
        return;
    }
    QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    const QString shown =
        m_hexBuffer.isEmpty() ? QStringLiteral("#") : QStringLiteral("#%1").arg(m_hexBuffer);
    clip->setText(shown);
    notifyStatus(QStringLiteral("Copied %1").arg(shown));
}

void SettingsUi::hexPaste()
{
    if (!m_hexActive) {
        return;
    }
    const QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    QString digits = normalizeHexDigits(clip->text());
    if (digits.size() > 8) {
        digits = digits.left(8);
    }
    if (digits.size() < 6 && !digits.isEmpty()) {
        // Keep partial paste so the user can finish typing.
    }
    m_hexBuffer = digits;
    refreshHexEditor();
    notifyStatus(digits.isEmpty() ? QStringLiteral("Clipboard has no hex")
                                  : QStringLiteral("Pasted #%1").arg(digits));
}

void SettingsUi::refreshHexEditor()
{
    if (!m_hexActive) {
        return;
    }
    QString err;
    LiveBoard hexBoard;
    (void)presentLive(hexBoard, QLatin1String(kLiveHex), buildHexDocument(), &err);
}

bool SettingsUi::hexSave(QString* error)
{
    if (!m_hexActive) {
        if (error) {
            *error = QStringLiteral("Hex editor is not open");
        }
        return false;
    }
    const auto parsed = parseHexDraft(m_hexBuffer);
    if (!parsed.ok) {
        const QString msg = QStringLiteral("Enter 6–8 hex digits");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    QColor c = parsed.rgb;
    c.setAlpha(parsed.setAlpha ? parsed.alpha : m_colorA);
    unbindEditorKeyboard();
    m_pages.closePage(QLatin1String(kLiveHex));
    m_hexActive = false;
    m_hexBuffer.clear();
    loadColorDraft(c);
    storeDraftPending();
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex #%1").arg(hexSeedFromColor(m_colorDraft)));
    return true;
}

bool SettingsUi::hexCancel(QString* error)
{
    Q_UNUSED(error);
    if (!m_hexActive) {
        return true;
    }
    unbindEditorKeyboard();
    m_pages.closePage(QLatin1String(kLiveHex));
    m_hexActive = false;
    m_hexBuffer.clear();
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex edit cancelled"));
    return true;
}

} // namespace gazer
