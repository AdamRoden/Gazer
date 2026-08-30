#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageSession.h"

#include <QColor>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveHex;

bool SettingsUi::openHexEditor(QString* error)
{
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
    QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    if (hex.startsWith(QLatin1Char('#'))) {
        hex = hex.mid(1);
    }
    m_hexBuffer = hex;
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
    initGrid(doc, 4, 7, 560, 700, 10, 16, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    const QString shown =
        m_hexBuffer.isEmpty() ? QStringLiteral("#") : QStringLiteral("#%1").arg(m_hexBuffer);
    PageCell display = cell(QStringLiteral("display"), shown, 0, 0, {}, pal.value, 4,
                            QStringLiteral("value"));
    grid.cells.push_back(display);

    const char* keys[] = {"1", "2", "3", "A", "4", "5", "6", "B",
                          "7", "8", "9", "C", "0", "D", "E", "F"};
    for (int i = 0; i < 16; ++i) {
        const int row = 1 + i / 4;
        const int col = i % 4;
        const QString k = QLatin1String(keys[i]);
        grid.cells.push_back(cell(QStringLiteral("h_%1").arg(k), k, row, col,
                                  QStringLiteral("settings.hex.digit.%1").arg(k), pal.key));
    }
    grid.cells.push_back(cell(QStringLiteral("back"), QStringLiteral("⌫"), 5, 0,
                              QStringLiteral("settings.hex.backspace"), pal.warn, 2));
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 5, 2,
                              QStringLiteral("settings.hex.clear"), pal.warn, 2));
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
    QString hex = m_hexBuffer;
    if (!hex.startsWith(QLatin1Char('#'))) {
        hex.prepend(QLatin1Char('#'));
    }
    const QColor c = AppSettings::parseColor(hex);
    if (!c.isValid()) {
        const QString msg = QStringLiteral("Invalid hex color");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    unbindEditorKeyboard();
    m_pages.closePage(QLatin1String(kLiveHex));
    m_hexActive = false;
    m_hexBuffer.clear();
    loadColorDraft(c);
    m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex %1").arg(c.name(QColor::HexArgb).toUpper()));
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
