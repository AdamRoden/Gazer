#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechHistory.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QColor>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QtGlobal>
#include <QVector>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using compose_detail::aboveKeyboardHeight;
using compose_detail::kHistoryCards;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;
using compose_detail::paintDanger;
using compose_detail::paintFlat;
using compose_detail::paintOutline;
using compose_detail::paintSurface;
using compose_detail::searchField;
using compose_detail::trackPx;
using SettingsPageBuild::makeNested;

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
    if (dt.date() == QDate::currentDate()) {
        return dt.toString(QStringLiteral("h:mm AP"));
    }
    return dt.toString(QStringLiteral("MMM d"));
}

bool ComposeUi::openHistory(QString* error)
{
    m_historyPage = 0;
    m_historySearch = true;
    m_historyQuery.clear();
    m_historyClearArm = false;
    const bool ok = presentLive(QString(kHistoryLiveId), buildHistoryDocument(), error);
    if (ok && isOpen()) {
        showComposeLayer(1);
    }
    return ok;
}

bool ComposeUi::clearHistory(QString* error)
{
    if (!m_historyClearArm) {
        m_historyClearArm = true;
        rebuildHistory();
        return true;
    }
    m_historyClearArm = false;
    if (!m_history.clear(error)) {
        rebuildHistory();
        return false;
    }
    m_historyPage = 0;
    rebuildHistory();
    notify(QStringLiteral("History cleared"));
    return true;
}

void ComposeUi::onHistoryReady(const QString& phrase, const QString& backend, const QString& modelId,
                               const QString& voiceId, const QString& clipPath)
{
    QString err;
    QString copied;
    (void)m_history.record(phrase, backend, modelId, voiceId, clipPath, &err, &copied);
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
    PageDocument doc = buildHistoryDocument();
    doc.id = QString(kHistoryLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

void ComposeUi::historyPage(int delta)
{
    m_historyClearArm = false;
    m_historyPage += delta;
    rebuildHistory();
}

void ComposeUi::historyGoto(int offset)
{
    m_historyClearArm = false;
    m_historyPage = offset;
    rebuildHistory();
}

bool ComposeUi::playHistory(const QString& id, QString* error)
{
    if (m_historyClearArm) {
        m_historyClearArm = false;
        rebuildHistory();
    }
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
    m_speech.speak(it->phrase, SpeakKind::Composed, false);
    return true;
}

bool ComposeUi::restoreHistory(const QString& id, QString* error)
{
    m_historyClearArm = false;
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
    if (hasLive(kHistoryLiveId)) {
        m_pages.closePage(QString(kHistoryLiveId));
    }
    notify(QStringLiteral("Restored"));
    return true;
}

bool ComposeUi::deleteHistory(const QString& id, QString* error)
{
    m_historyClearArm = false;
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
    initTopOverlay(doc, 1, 4, {1, 1, 1, 1}, 8, 16, theme, aboveKeyboardHeight(m_composeAuthored),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {trackPx(72), trackPx(72), PageTrackSize::starWeight(), trackPx(72)};
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);

    QVector<SpeechHistoryItem> items;
    const QString query = m_historyQuery.trimmed();
    for (const SpeechHistoryItem& it : m_history.items()) {
        if (query.isEmpty() || it.phrase.contains(query, Qt::CaseInsensitive)) {
            items.push_back(it);
        }
    }
    const int maxOffset = qMax(0, items.size() - kHistoryCards);
    m_historyPage = qBound(0, m_historyPage, maxOffset);
    const int start = m_historyPage;
    const QString caption = items.isEmpty()
                                ? (query.isEmpty() ? QStringLiteral("No history yet")
                                                   : QStringLiteral("No matches"))
                                : QString();

    PageGrid title =
        compose_detail::speakTitleBar(QStringLiteral("History"), false, key);
    if (!caption.isEmpty() && !title.cells.isEmpty()) {
        title.cells[0].caption = caption;
    }
    grid.subGrids.push_back(std::move(title));

    grid.cells.push_back(searchField(QStringLiteral("history_search"), m_historyQuery,
                                     QStringLiteral("Search speech history..."), 1, 0, 1, key,
                                     accent));

    PageGrid body = overlayBody(2, 1);
    PageGrid list = overlayList(1, kHistoryCards);
    list.gapPx = 8;
    const QColor rim = ThemeColors::mix(value, ThemeColors::contrastOn(value), 0.28);
    for (int i = 0; i < kHistoryCards; ++i) {
        const int idx = start + i;
        if (idx >= items.size()) {
            PageCell empty = cell(QStringLiteral("hist_%1").arg(i), {}, i, 0, {}, QColor(), 1,
                                  QStringLiteral("label"));
            paintFlat(empty);
            list.cells.push_back(std::move(empty));
            continue;
        }
        const SpeechHistoryItem& it = items.at(idx);
        PageGrid row = makeNested(QStringLiteral("hist_%1").arg(i), i, 0, 1, 5, 8);
        row.marginPx = 6;
        row.columnTracks = {PageTrackSize::starWeight(), trackPx(108), trackPx(104), trackPx(128),
                            trackPx(64)};
        row.style.radius = PageBox::all(12);
        row.style.background = value;
        row.style.borderColor = rim;
        row.style.thickness = PageBox::all(1);
        PageCell phrase = cell(QStringLiteral("h_%1").arg(i), ellipsis(it.phrase, 48), 0, 0, {},
                               QColor(), 1, QStringLiteral("label"));
        phrase.textStyle = QStringLiteral("row");
        paintFlat(phrase);
        row.cells.push_back(std::move(phrase));
        PageCell when = cell(QStringLiteral("when_%1").arg(i), historyWhen(it), 0, 1, {}, QColor(),
                             1, QStringLiteral("label"));
        when.textStyle = QStringLiteral("caption");
        paintFlat(when);
        row.cells.push_back(std::move(when));
        PageCell play = cell(QStringLiteral("p_%1").arg(i), QStringLiteral("Play"), 0, 2,
                             QStringLiteral("history.play.%1").arg(it.id), key);
        paintSurface(play, key);
        row.cells.push_back(std::move(play));
        PageCell restore = cell(QStringLiteral("r_%1").arg(i), QStringLiteral("Restore"), 0, 3,
                                QStringLiteral("history.restore.%1").arg(it.id), QColor());
        paintOutline(restore, accent);
        restore.style.radius = PageBox::all(12);
        row.cells.push_back(std::move(restore));
        PageCell remove = cell(QStringLiteral("d_%1").arg(i), {}, 0, 4,
                               QStringLiteral("history.delete.%1").arg(it.id), QColor(), 1, {}, {},
                               QStringLiteral("close"));
        paintDanger(remove, warn);
        row.cells.push_back(std::move(remove));
        list.subGrids.push_back(std::move(row));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kHistoryCards, items.size(), key,
                                            QStringLiteral("history.list.prev"),
                                            QStringLiteral("history.list.next")));
    grid.subGrids.push_back(std::move(body));

    PageCell clear = cell(m_historyClearArm ? QStringLiteral("clear_all_confirm")
                                            : QStringLiteral("clear_all"),
                          m_historyClearArm ? QStringLiteral("Clear everything?")
                                            : QStringLiteral("Clear All History"),
                          3, 0, QStringLiteral("history.clear"), QColor());
    paintDanger(clear, warn);
    grid.cells.push_back(std::move(clear));
    return doc;
}

} // namespace gazer
