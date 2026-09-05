#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenClient.h"
#include "assist/PhraseService.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechHistory.h"
#include "assist/TtsService.h"
#include "assist/VoiceCatalog.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QtGlobal>
#include <QVector>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::closeSelf;
using SettingsPageBuild::initTopOverlay;

constexpr int kHistoryVisible = 10;
constexpr int kHistoryScrollSlots = 10;

QString historyVoiceName(const SpeechHistoryItem& it, const QVector<VoiceCatalog::Voice>& eleven,
                         const QVector<TtsService::VoiceInfo>& sapi)
{
    if (!it.voiceId.isEmpty()) {
        for (const VoiceCatalog::Voice& v : eleven) {
            if (v.id == it.voiceId) {
                return v.name;
            }
        }
        for (const TtsService::VoiceInfo& s : sapi) {
            if (s.token == it.voiceId) {
                return s.name;
            }
        }
    }
    if (it.backend == QLatin1String("eleven")) {
        return QStringLiteral("ElevenLabs");
    }
    if (it.backend == QLatin1String("sapi")) {
        return QStringLiteral("SAPI");
    }
    return it.backend;
}

QString historyWhen(const SpeechHistoryItem& it)
{
    QDateTime dt = QDateTime::fromString(it.atIso, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(it.atIso, Qt::ISODateWithMs);
    }
    if (!dt.isValid()) {
        return it.atIso;
    }
    if (dt.timeSpec() == Qt::UTC || it.atIso.endsWith(QLatin1Char('Z'))) {
        dt = dt.toLocalTime();
    }
    return dt.toString(QStringLiteral("MMM d, h:mm AP"));
}

QString historyCaption(const SpeechHistoryItem& it, const QVector<VoiceCatalog::Voice>& eleven,
                       const QVector<TtsService::VoiceInfo>& sapi)
{
    const QString voice = historyVoiceName(it, eleven, sapi);
    const QString when = historyWhen(it);
    if (voice.isEmpty()) {
        return when;
    }
    if (when.isEmpty()) {
        return voice;
    }
    return QStringLiteral("%1  ·  %2").arg(voice, when);
}

PageCell scrollHit(const QString& id, int row, int rowSpan, const QString& command,
                   const QColor& bg, double radius)
{
    PageCell c = cell(id, {}, row, 0, command, bg, 1, command.isEmpty() ? QStringLiteral("label")
                                                                       : QString());
    c.rowSpan = qMax(1, rowSpan);
    c.style.thickness = PageBox::all(0);
    c.style.radius = PageBox::all(radius);
    c.style.borderColor = QColor(0, 0, 0, 0);
    return c;
}

bool ComposeUi::openHistory(QString* error)
{
    m_historyPage = 0;
    return presentLive(QString(kHistoryLiveId), buildHistoryDocument(), error);
}

void ComposeUi::onHistoryReady(const QString& phrase, const QString& backend, const QString& modelId,
                               const QString& voiceId, const QString& mpegPath)
{
    QString err;
    QString copied;
    (void)m_history.record(phrase, backend, modelId, voiceId, mpegPath, &err, &copied);
    if (!copied.isEmpty()) {
        m_speech.keepGeneratedClip(copied);
    }
    if (hasLive(kHistoryLiveId)) {
        m_historyPage = 0;
        rebuildHistory();
    }
}

void ComposeUi::rebuildHistory()
{
    if (!hasLive(kHistoryLiveId)) {
        return;
    }
    QString err;
    (void)presentLive(QString(kHistoryLiveId), buildHistoryDocument(), &err);
}

void ComposeUi::historyPage(int delta)
{
    m_historyPage += delta;
    rebuildHistory();
}

void ComposeUi::historyGoto(int offset)
{
    m_historyPage = offset;
    rebuildHistory();
}

bool ComposeUi::playHistory(const QString& id, QString* error)
{
    const SpeechHistoryItem* it = m_history.find(id);
    if (!it) {
        return false;
    }
    const QString path = m_history.clipPath(it->id);
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        if (m_speech.playFile(path)) {
            return true;
        }
    }
    return m_phrases.speak(it->phrase, SpeakKind::Composed, error, false);
}

bool ComposeUi::restoreHistory(const QString& id, QString* error)
{
    const SpeechHistoryItem* it = m_history.find(id);
    if (!it) {
        if (error) {
            *error = QStringLiteral("Unknown history item");
        }
        return false;
    }
    m_buffer.clear();
    m_buffer.insert(it->phrase);
    refresh();
    notify(QStringLiteral("Restored"));
    return true;
}

bool ComposeUi::deleteHistory(const QString& id, QString* error)
{
    stopSpeech();
    if (!m_history.remove(id, error)) {
        return false;
    }
    rebuildHistory();
    notify(QStringLiteral("Deleted"));
    return true;
}

PageDocument ComposeUi::buildHistoryDocument()
{
    PageDocument doc;
    doc.id = QString(kHistoryLiveId);
    doc.name = QStringLiteral("History");
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 8, 2, {1.0, 10.0}, 8, 16, theme, QStringLiteral("A_ScreenHeight"));
    PageGrid& grid = doc.grids[0];
    const QColor key = theme.bgSurface.isValid() ? theme.bgSurface : QColor(40, 40, 44);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);

    const auto& items = m_history.items();
    const QVector<VoiceCatalog::Voice> elevenVoices =
        VoiceCatalog::parseElevenCache(ElevenClient::loadVoicesCache());
    const QVector<TtsService::VoiceInfo> sapiVoices = m_tts.listVoices();
    const int maxOffset = qMax(0, items.size() - kHistoryVisible);
    m_historyPage = qBound(0, m_historyPage, maxOffset);
    const int start = m_historyPage;
    const QString caption = items.isEmpty()
                                ? QStringLiteral("No history yet")
                                : QStringLiteral("%1–%2 of %3")
                                      .arg(start + 1)
                                      .arg(qMin(start + kHistoryVisible, items.size()))
                                      .arg(items.size());
    grid.cells.push_back(cell(QStringLiteral("caption"), caption, 0, 0, {}, value, 6,
                              QStringLiteral("value")));
    PageCell done = cell(QStringLiteral("close"), QStringLiteral("Done"), 0, 6, {}, warn, 2);
    done.actions.push_back(closeSelf());
    grid.cells.push_back(done);

    PageGrid list;
    list.id = QStringLiteral("list");
    list.nested = true;
    list.row = 1;
    list.col = 0;
    list.colSpan = 7;
    list.rows = kHistoryVisible;
    list.columns = 5;
    list.gapPx = 8;
    for (int i = 0; i < kHistoryVisible; ++i) {
        const int idx = start + i;
        if (idx >= items.size()) {
            list.cells.push_back(cell(QStringLiteral("d_%1").arg(i), {}, i, 0, {}, value, 1,
                                      QStringLiteral("label")));
            list.cells.push_back(cell(QStringLiteral("h_%1").arg(i), {}, i, 1, {}, value, 3,
                                      QStringLiteral("label")));
            list.cells.push_back(cell(QStringLiteral("r_%1").arg(i), {}, i, 4, {}, value, 1,
                                      QStringLiteral("label")));
            continue;
        }
        const SpeechHistoryItem& it = items[idx];
        list.cells.push_back(cell(QStringLiteral("d_%1").arg(i), {}, i, 0,
                                  QStringLiteral("history.delete.%1").arg(it.id), warn, 1, {}, {},
                                  QStringLiteral("delete")));
        list.cells.push_back(cell(QStringLiteral("h_%1").arg(i), ellipsis(it.phrase, 64), i, 1,
                                  QStringLiteral("history.play.%1").arg(it.id),
                                  it.backend == QLatin1String("eleven") ? accent : key, 3, {},
                                  historyCaption(it, elevenVoices, sapiVoices)));
        list.cells.push_back(cell(QStringLiteral("r_%1").arg(i), QStringLiteral("Restore"), i, 4,
                                  QStringLiteral("history.restore.%1").arg(it.id), key));
    }
    grid.subGrids.push_back(std::move(list));

    PageGrid scroll;
    scroll.id = QStringLiteral("scroll");
    scroll.nested = true;
    scroll.row = 1;
    scroll.col = 7;
    scroll.colSpan = 1;
    scroll.rows = kHistoryScrollSlots;
    scroll.columns = 1;
    scroll.gapPx = 0;
    scroll.marginPx = 4;
    scroll.style.background = key;
    scroll.style.thickness = PageBox::all(0);
    scroll.style.radius = PageBox::all(12);
    const QColor clear(0, 0, 0, 0);
    if (maxOffset > 0) {
        const int thumbStart =
            qBound(0,
                   int(qRound(double(m_historyPage) * double(kHistoryScrollSlots - 1)
                              / double(maxOffset))),
                   kHistoryScrollSlots - 1);
        for (int i = 0; i < kHistoryScrollSlots; ++i) {
            if (i == thumbStart) {
                scroll.cells.push_back(
                    scrollHit(QStringLiteral("thumb"), i, 1,
                              QStringLiteral("history.list.goto.%1").arg(start), accent, 10));
                continue;
            }
            const int pos = qBound(0,
                                   int(qRound(double(i) * double(maxOffset)
                                              / double(kHistoryScrollSlots - 1))),
                                   maxOffset);
            scroll.cells.push_back(scrollHit(QStringLiteral("sb_%1").arg(i), i, 1,
                                             QStringLiteral("history.list.goto.%1").arg(pos),
                                             clear, 0));
        }
    }
    grid.subGrids.push_back(std::move(scroll));
    return doc;
}

} // namespace gazer
