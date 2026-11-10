#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"
#include "assist/MouseDwellMove.h"
#include "assist/SpeechSecrets.h"
#include "layout/PageSession.h"

#include <QClipboard>
#include <QColor>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::kLiveSpeechKey;
using SettingsUiInternal::kLiveSpeechLink;

constexpr int kLinkVerifyTries = 4;
constexpr int kLinkVerifyDelayMs = 400;
/// Dock menu box (`main.xml` drawer `size`).
constexpr int kMenuBarWidth = 725;
constexpr int kMenuBarHeight = 150;

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

QByteArray base64Url(const QByteArray& raw)
{
    return raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

bool looksLikeApiKey(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("^sk_[0-9a-f]{20,}[0-9a-z_]*$"));
    return re.match(text).hasMatch();
}

QString handoffPage()
{
    return QStringLiteral(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Gazer</title></head>"
        "<body><p>You can close this tab and return to Gazer.</p>"
        "<script>window.close();</script></body></html>");
}

class KeyHandoff final : public QObject {
public:
    explicit KeyHandoff(QObject* parent)
        : QObject(parent)
    {
    }

    ~KeyHandoff() override
    {
        if (m_server) {
            m_server->close();
        }
    }

    bool listen()
    {
        m_server = new QTcpServer(this);
        if (!m_server->listen(QHostAddress::LocalHost, 8484)) {
            return false;
        }
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            while (m_server && m_server->hasPendingConnections()) {
                accept(m_server->nextPendingConnection());
            }
        });
        auto* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, &QObject::deleteLater);
        timer->start(3 * 60 * 1000);
        return true;
    }

private:
    static void respond(QTcpSocket* sock)
    {
        if (!sock || sock->property("gazerSent").toBool()) {
            return;
        }
        sock->setProperty("gazerSent", true);
        const QByteArray body = handoffPage().toUtf8();
        const QByteArray head = QByteArrayLiteral("HTTP/1.1 200 OK\r\n"
                                                   "Content-Type: text/html; charset=utf-8\r\n"
                                                   "Connection: close\r\nContent-Length: ")
                                + QByteArray::number(body.size())
                                + QByteArrayLiteral("\r\n\r\n");
        sock->write(head);
        sock->write(body);
        sock->disconnectFromHost();
    }

    void accept(QTcpSocket* sock)
    {
        if (!sock) {
            return;
        }
        connect(sock, &QTcpSocket::readyRead, sock, [sock]() {
            sock->readAll();
            respond(sock);
        });
        if (sock->bytesAvailable() > 0) {
            sock->readAll();
            respond(sock);
        }
        QTimer::singleShot(30000, sock, [sock]() {
            sock->disconnectFromHost();
            sock->deleteLater();
        });
    }

    QTcpServer* m_server = nullptr;
};

} // namespace

void SettingsUi::endLinkAttempt()
{
    // Clear the armed flag before setArmed so the disarm signal does not start a clipboard check.
    m_keyClickArmed = false;
    m_keyClickLanded = false;
    m_keyChecking = false;
    m_keyLink = false;
    m_linkCheckGen = -1;
    ++m_linkVerifyGen;
    if (m_mouseDwell && m_mouseDwell->isArmed()
        && m_mouseDwell->armPurpose() == MouseDwellMove::ArmPurpose::CursorMoveLeftClick) {
        m_mouseDwell->setArmed(false);
    }
    stopKeyHandoff();
}

void SettingsUi::stopKeyHandoff()
{
    QObject* handoff = m_handoff;
    m_handoff = nullptr;
    delete handoff;
}

void SettingsUi::closeSettingsForLink()
{
    if (m_pages.hasPage(QStringLiteral("main_settings_host"))) {
        m_pages.closePage(QStringLiteral("main_settings_host"));
    }
}

void SettingsUi::returnToSpeakSettings()
{
    QString err;
    if (!m_pages.openPage(QStringLiteral("main_settings_host"), &err)) {
        notifyStatus(err.isEmpty() ? QStringLiteral("Could not open Settings") : err);
        return;
    }
    PageAction hop;
    hop.type = PageActionType::HostPage;
    hop.targetId = QStringLiteral("main_settings_speak");
    hop.hostId = QStringLiteral("main_settings_host");
    if (!m_pages.applyNav(hop, QStringLiteral("main_settings_host"), {}, &err) && !err.isEmpty()) {
        notifyStatus(err);
    }
}

void SettingsUi::startLinkBrowser()
{
    QByteArray verifier(32, '\0');
    auto* words = reinterpret_cast<quint32*>(verifier.data());
    for (int i = 0; i < 8; ++i) {
        words[i] = QRandomGenerator::system()->generate();
    }
    const QByteArray challenge =
        base64Url(QCryptographicHash::hash(base64Url(verifier), QCryptographicHash::Sha256));
    const QByteArray state = base64Url(QByteArray::number(QRandomGenerator::system()->generate64()));
    const QString url = challenge.isEmpty()
                            ? ElevenRequest::keyQuickstartUrl()
                            : ElevenRequest::keyHandoffUrl(QString::fromLatin1(challenge),
                                                           QString::fromLatin1(state));

    stopKeyHandoff();
    if (m_audioWatch) {
        auto* handoff = new KeyHandoff(m_audioWatch.get());
        if (handoff->listen()) {
            m_handoff = handoff;
            QObject::connect(handoff, &QObject::destroyed, m_audioWatch.get(), [this, handoff]() {
                if (m_handoff == handoff) {
                    m_handoff = nullptr;
                }
            });
        } else {
            delete handoff;
        }
    }
    QDesktopServices::openUrl(QUrl(url));
}

PageDocument SettingsUi::buildLinkDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveSpeechLink);
    doc.name = QStringLiteral("Create key");
    initGrid(doc, 2, 1, kMenuBarWidth, kMenuBarHeight, 10, 10, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    grid.desktopMode = true;
    grid.anchor = PageAnchor::Bottom;
    grid.size.x = PageDim::pixels(kMenuBarWidth);
    grid.size.y = PageDim::pixels(kMenuBarHeight);
    const EditorSwatch pal = editorSwatch();
    const QString status = m_keyLinkStatus.isEmpty()
                               ? QStringLiteral("Dwell Click, then Authorize in the browser")
                               : m_keyLinkStatus;
    PageCell click = cell(QStringLiteral("link_click"), QStringLiteral("Click"), 0, 0,
                          QStringLiteral("mouseLeftClickAtGaze"), pal.save, 1, {}, status,
                          QStringLiteral("mouseLeftClickAtGaze"));
    click.activeState = QStringLiteral("mouseLeftClickAtGaze");
    grid.cells.push_back(std::move(click));
    grid.cells.push_back(cell(QStringLiteral("link_cancel"), QStringLiteral("Cancel"), 0, 1,
                              QStringLiteral("settings.speech.linkCancel"), pal.cancel));
    return doc;
}

void SettingsUi::refreshLinkPage()
{
    if (!m_keyLinkPage.active) {
        return;
    }
    QString err;
    (void)presentLive(m_keyLinkPage, QLatin1String(kLiveSpeechLink), buildLinkDocument(), &err);
}

void SettingsUi::cancelLinkPage()
{
    if (!m_keyLinkPage.active) {
        return;
    }
    endLinkAttempt();
    closeLive(m_keyLinkPage);
    returnToSpeakSettings();
}

void SettingsUi::finishLinkVerified()
{
    endLinkAttempt();
    if (m_keyLinkPage.active) {
        closeLive(m_keyLinkPage);
    }
    returnToSpeakSettings();
}

void SettingsUi::abandonLinkPageIfClosed()
{
    if (!m_keyLinkPage.active || m_pages.hasPage(m_keyLinkPage.pageId)) {
        return;
    }
    endLinkAttempt();
    m_keyLinkPage.reset();
}

void SettingsUi::onDwellMoveMoved(const QPoint& pos)
{
    if (m_keyClickArmed) {
        m_keyClickLanded = true;
        return;
    }
    onColorAimMoved(pos);
}

void SettingsUi::onDwellMoveArmed(bool armed)
{
    if (!armed) {
        cancelEyedropper();
        finishLinkClick();
        return;
    }
    noteLinkClickArmed();
}

void SettingsUi::noteLinkClickArmed()
{
    if (!m_keyLinkPage.active || !m_mouseDwell) {
        return;
    }
    if (m_mouseDwell->armPurpose() != MouseDwellMove::ArmPurpose::CursorMoveLeftClick) {
        m_keyClickArmed = false;
        m_keyClickLanded = false;
        return;
    }
    m_keyClickArmed = true;
    m_keyClickLanded = false;
    ++m_linkVerifyGen;
}

void SettingsUi::finishLinkClick()
{
    if (!m_keyClickArmed) {
        return;
    }
    const bool landed = m_keyClickLanded;
    m_keyClickArmed = false;
    m_keyClickLanded = false;
    if (!m_keyLinkPage.active) {
        return;
    }
    if (!landed) {
        m_keyLinkStatus = QStringLiteral("Click cancelled. Dwell Click, then Authorize.");
        refreshLinkPage();
        return;
    }
    m_keyLinkStatus = QStringLiteral("Checking the clipboard\u2026");
    refreshLinkPage();
    scheduleLinkVerify();
}

void SettingsUi::scheduleLinkVerify()
{
    if (!m_audioWatch) {
        m_keyLinkStatus = QStringLiteral("Could not check the clipboard");
        refreshLinkPage();
        notifyStatus(m_keyLinkStatus);
        return;
    }
    const int gen = m_linkVerifyGen;
    QTimer::singleShot(kLinkVerifyDelayMs, m_audioWatch.get(), [this, gen]() {
        pollLinkClipboard(gen, 0);
    });
}

void SettingsUi::pollLinkClipboard(int gen, int attempt)
{
    if (gen != m_linkVerifyGen || !m_keyLinkPage.active || m_keyChecking) {
        return;
    }
    const QString clip = clipboardApiKey();
    if (looksLikeApiKey(clip)) {
        m_keyBuffer = clip;
        m_keyLink = true;
        m_keyChecking = true;
        m_linkCheckGen = gen;
        m_keyLinkStatus = QStringLiteral("Checking the ElevenLabs key");
        refreshLinkPage();
        m_eleven.validateApiKey(clip);
        notifyStatus(m_keyLinkStatus);
        return;
    }
    if (attempt + 1 < kLinkVerifyTries && m_audioWatch) {
        QTimer::singleShot(kLinkVerifyDelayMs, m_audioWatch.get(), [this, gen, attempt]() {
            pollLinkClipboard(gen, attempt + 1);
        });
        return;
    }
    m_keyLinkStatus = QStringLiteral("No key yet. Dwell Click on Authorize.");
    refreshLinkPage();
    notifyStatus(m_keyLinkStatus);
}

bool SettingsUi::linkSpeechKey(QString* error)
{
    if (QClipboard* clip = QGuiApplication::clipboard()) {
        clip->clear();
    }
    if (m_key.active) {
        m_keyChecking = false;
        closeLive(m_key);
    }
    closeSettingsForLink();
    endLinkAttempt();
    startLinkBrowser();
    m_keyLinkStatus = QStringLiteral("Dwell Click, then Authorize in the browser");
    if (!presentLive(m_keyLinkPage, QLatin1String(kLiveSpeechLink), buildLinkDocument(), error)) {
        m_keyLinkPage.reset();
        stopKeyHandoff();
        returnToSpeakSettings();
        return false;
    }
    notifyStatus(m_keyLinkStatus);
    return true;
}

bool SettingsUi::openSpeechKeyBoard(QString* error)
{
    m_keyChecking = false;
    m_keyBuffer.clear();
    QString existing;
    if (m_secrets.load(&existing)) {
        m_keyBuffer = existing;
    }
    if (!presentLive(m_key, QLatin1String(kLiveSpeechKey), buildSpeechKeyDocument(), error)) {
        m_key.reset();
        return false;
    }
    notifyStatus(m_keyBuffer.isEmpty() ? QStringLiteral("Paste a key, or create one")
                                       : QStringLiteral("Save to keep this key"));
    return true;
}

PageDocument SettingsUi::buildSpeechKeyDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveSpeechKey);
    doc.name = QStringLiteral("ElevenLabs key");
    initGrid(doc, 6, 3, 900, 360, 10, 16, m_settings.resolvedTheme());
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
    } else {
        shown = QStringLiteral("No API key");
    }
    grid.cells.push_back(cell(QStringLiteral("display"), shown, 0, 0, {}, pal.value, 6,
                              QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("paste"), QStringLiteral("Paste"), 1, 0,
                              QStringLiteral("settings.speech.key.paste"), pal.save, 3, {}, {},
                              QStringLiteral("contentPaste")));
    grid.cells.push_back(cell(QStringLiteral("create"), QStringLiteral("Create key"), 1, 3,
                              QStringLiteral("settings.speech.linkKey"), pal.add, 3));
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 2, 0,
                              QStringLiteral("settings.speech.key.clear"), pal.warn, 2));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 2, 2,
                              QStringLiteral("settings.speech.key.save"), pal.save, 2));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 2, 4,
                              QStringLiteral("settings.speech.key.cancel"), pal.cancel, 2));
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
    QString err;
    if (!m_secrets.clear(&err)) {
        notifyStatus(err.isEmpty() ? QStringLiteral("Could not clear API key") : err);
        return;
    }
    m_keyBuffer.clear();
    apply(true);
    refreshSpeechKeyBoard();
    notifyStatus(QStringLiteral("API key cleared"));
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
        QString err;
        if (!m_secrets.clear(&err)) {
            if (error) {
                *error = err.isEmpty() ? QStringLiteral("Could not clear API key") : err;
            }
            notifyStatus(err.isEmpty() ? QStringLiteral("Could not clear API key") : err);
            return false;
        }
        m_keyBuffer.clear();
        closeLive(m_key);
        apply(true);
        notifyStatus(QStringLiteral("API key cleared"));
        return true;
    }
    m_keyChecking = true;
    m_keyLink = false;
    m_linkCheckGen = -1;
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
    const bool linkCurrent = m_keyLink && m_linkCheckGen == m_linkVerifyGen;
    const bool paste = m_key.active;
    if (!linkCurrent && !paste) {
        return false;
    }
    m_keyChecking = false;
    if (linkCurrent) {
        m_keyLink = false;
        m_linkCheckGen = -1;
    }
    if (!ok) {
        if (linkCurrent) {
            m_keyLinkStatus = error.isEmpty() ? QStringLiteral("Key rejected. Dwell Click to try again.")
                                              : error;
            refreshLinkPage();
        }
        if (paste) {
            refreshSpeechKeyBoard();
        }
        notifyStatus(error.isEmpty() ? QStringLiteral("API key rejected") : error);
        return false;
    }
    QString err;
    if (!m_secrets.store(m_keyBuffer, &err)) {
        if (linkCurrent) {
            m_keyLinkStatus = err.isEmpty() ? QStringLiteral("Could not save API key") : err;
            refreshLinkPage();
        }
        if (paste) {
            refreshSpeechKeyBoard();
        }
        notifyStatus(err.isEmpty() ? QStringLiteral("Could not save API key") : err);
        return false;
    }
    m_keyBuffer.clear();
    if (paste) {
        closeLive(m_key);
    }
    apply(true);
    notifyStatus(QStringLiteral("ElevenLabs key saved"));
    if (linkCurrent) {
        finishLinkVerified();
    }
    return true;
}

} // namespace gazer
