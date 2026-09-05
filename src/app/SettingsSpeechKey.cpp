#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "assist/ElevenClient.h"
#include "assist/SpeechSecrets.h"
#include "layout/PageSession.h"

#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QString>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveSpeechKey;

namespace {

QString clipboardApiKey()
{
    const QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return {};
    }
    QString text = clip->text().trimmed();
    const bool quoted =
        text.size() >= 2
        && ((text.front() == QLatin1Char('"') && text.back() == QLatin1Char('"'))
            || (text.front() == QLatin1Char('\'') && text.back() == QLatin1Char('\'')));
    if (quoted) {
        text = text.mid(1, text.size() - 2).trimmed();
    }
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (!c.isSpace()) {
            out += c;
        }
    }
    return out;
}

} // namespace

bool SettingsUi::openSpeechKeyBoard(QString* error)
{
    m_keyChecking = false;
    m_keyBuffer = clipboardApiKey();
    if (!presentLive(m_key, QLatin1String(kLiveSpeechKey), buildSpeechKeyDocument(), error)) {
        m_key.reset();
        return false;
    }
    notifyStatus(m_keyBuffer.isEmpty() ? QStringLiteral("Copy the API key, then Paste")
                                       : QStringLiteral("Pasted API key — Save to apply"));
    return true;
}

PageDocument SettingsUi::buildSpeechKeyDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveSpeechKey);
    doc.name = QStringLiteral("ElevenLabs key");
    initGrid(doc, 3, 3, 900, 360, 10, 16, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    QString shown;
    if (m_keyChecking) {
        shown = QStringLiteral("Checking\u2026");
    } else if (!m_keyBuffer.isEmpty()) {
        shown = QString(m_keyBuffer.size(), QChar(0x2022));
        if (m_keyBuffer.size() >= 4) {
            shown = shown.left(shown.size() - 4) + m_keyBuffer.right(4);
        }
    } else if (m_secrets.hasKey()) {
        shown = QStringLiteral("\u2022\u2022\u2022\u2022%1").arg(m_secrets.lastFour());
    } else {
        shown = QStringLiteral("Paste API key from clipboard");
    }
    grid.cells.push_back(cell(QStringLiteral("display"), shown, 0, 0, {}, pal.value, 3,
                              QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("paste"), QStringLiteral("Paste"), 1, 0,
                              QStringLiteral("settings.speech.key.paste"), pal.save, 3, {}, {},
                              QStringLiteral("contentPaste")));
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 2, 0,
                              QStringLiteral("settings.speech.key.clear"), pal.warn));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 2, 1,
                              QStringLiteral("settings.speech.key.save"), pal.save));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 2, 2,
                              QStringLiteral("settings.speech.key.cancel"), pal.cancel));
    return doc;
}

void SettingsUi::refreshSpeechKeyBoard()
{
    if (!m_key.active) {
        return;
    }
    QString err;
    (void)presentLive(m_key, QLatin1String(kLiveSpeechKey), buildSpeechKeyDocument(), &err);
}

void SettingsUi::speechKeyPaste()
{
    if (!m_key.active || m_keyChecking) {
        return;
    }
    const QString text = clipboardApiKey();
    if (text.isEmpty()) {
        notifyStatus(QStringLiteral("Clipboard is empty"));
        return;
    }
    m_keyBuffer = text;
    refreshSpeechKeyBoard();
    notifyStatus(QStringLiteral("Pasted API key"));
}

void SettingsUi::speechKeyClear()
{
    if (!m_key.active || m_keyChecking) {
        return;
    }
    m_keyBuffer.clear();
    refreshSpeechKeyBoard();
}

bool SettingsUi::speechKeySave(QString* error)
{
    if (!m_key.active) {
        if (error) {
            *error = QStringLiteral("Key board is not open");
        }
        return false;
    }
    if (m_keyBuffer.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Paste a key first");
        }
        notifyStatus(QStringLiteral("Paste a key first"));
        return false;
    }
    m_keyChecking = true;
    refreshSpeechKeyBoard();
    m_eleven.validateApiKey(m_keyBuffer);
    return true;
}

void SettingsUi::speechKeyCancel()
{
    if (!m_key.active) {
        return;
    }
    m_keyChecking = false;
    m_keyBuffer.clear();
    closeLive(m_key);
}

bool SettingsUi::clearSpeechKey(QString* error)
{
    if (!m_secrets.clear(error)) {
        return false;
    }
    apply(true);
    return true;
}

bool SettingsUi::onSpeechKeyValidated(bool ok, const QString& error)
{
    m_keyChecking = false;
    if (!m_key.active) {
        return false;
    }
    if (!ok) {
        refreshSpeechKeyBoard();
        notifyStatus(error.isEmpty() ? QStringLiteral("API key rejected") : error);
        return false;
    }
    QString err;
    if (!m_secrets.store(m_keyBuffer, &err)) {
        refreshSpeechKeyBoard();
        notifyStatus(err.isEmpty() ? QStringLiteral("Could not save API key") : err);
        return false;
    }
    m_keyBuffer.clear();
    closeLive(m_key);
    apply(true);
    notifyStatus(QStringLiteral("ElevenLabs key saved"));
    return true;
}

} // namespace gazer
