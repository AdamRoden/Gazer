#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechSecrets.h"
#include "assist/TtsService.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QColor>
#include <QJsonObject>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::closeSelf;
using SettingsPageBuild::initTopOverlay;
using compose_detail::kOverlayListRows;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;

bool ComposeUi::openVoices(QString* error)
{
    m_voicePage = 0;
    m_voiceSearch = false;
    m_voiceQuery.clear();
    m_langBrowse = false;
    requestCatalogIfNeeded();
    return presentLive(QString(kVoicesLiveId), buildVoicesDocument(), error);
}

void ComposeUi::onCatalogReady(bool ok, const QString& error)
{
    Q_UNUSED(ok);
    Q_UNUSED(error);
    m_voicesLoading = false;
    if (hasLive(kVoicesLiveId)) {
        rebuildVoices();
    }
}

void ComposeUi::requestCatalogIfNeeded()
{
    if (!elevenMode() || !m_secrets.hasKey()) {
        return;
    }
    const QJsonObject cache = ElevenClient::loadVoicesCache();
    if (ElevenClient::voicesCacheFresh(cache)
        && cache.contains(QStringLiteral("voices"))) {
        return;
    }
    QString key;
    if (!m_secrets.load(&key) || key.trimmed().isEmpty()) {
        return;
    }
    m_voicesLoading = true;
    m_eleven.refreshCatalog(key);
}

QVector<VoiceCatalog::Voice> ComposeUi::currentVoices() const
{
    if (elevenMode()) {
        return VoiceCatalog::parseElevenCache(ElevenClient::loadVoicesCache());
    }
    QVector<VoiceCatalog::Voice> out;
    const auto sap = m_tts.listVoices();
    out.reserve(sap.size());
    for (const TtsService::VoiceInfo& s : sap) {
        VoiceCatalog::Voice v;
        v.id = s.token;
        v.name = s.name;
        v.gender = s.gender;
        v.language = s.language;
        v.eleven = false;
        out.push_back(v);
    }
    return out;
}

QVector<VoiceCatalog::Voice> ComposeUi::filteredVoices() const
{
    auto all = currentVoices();
    auto filtered = VoiceCatalog::filter(all, m_genderFilter, m_settings.speechLangFilter);
    const QString query = m_voiceQuery.trimmed();
    if (!query.isEmpty()) {
        QVector<VoiceCatalog::Voice> named;
        named.reserve(filtered.size());
        for (const VoiceCatalog::Voice& v : filtered) {
            if (v.name.contains(query, Qt::CaseInsensitive)) {
                named.push_back(v);
            }
        }
        filtered = std::move(named);
    }
    if (elevenMode()) {
        VoiceCatalog::sortFavoritesFirst(&filtered, m_settings.elevenFavoriteVoiceIds);
    }
    return filtered;
}

void ComposeUi::rebuildVoices()
{
    if (!hasLive(kVoicesLiveId)) {
        return;
    }
    QString err;
    (void)presentLive(QString(kVoicesLiveId), buildVoicesDocument(), &err);
}

void ComposeUi::selectVoice(const QString& encodedId)
{
    const QString id = VoiceCatalog::decodeId(encodedId).trimmed();
    if (id.isEmpty()) {
        return;
    }
    if (elevenMode()) {
        m_settings.elevenVoiceId = id;
    } else {
        m_settings.sapiVoiceToken = id;
    }
    apply();
}

void ComposeUi::toggleFavorite(const QString& encodedId)
{
    if (!elevenMode()) {
        return;
    }
    QString id = VoiceCatalog::decodeId(encodedId).trimmed();
    if (id.isEmpty()) {
        id = currentVoiceId();
    }
    if (id.isEmpty()) {
        return;
    }
    QStringList& favs = m_settings.elevenFavoriteVoiceIds;
    const int i = favs.indexOf(id);
    if (i >= 0) {
        favs.removeAt(i);
    } else {
        favs.push_front(id);
    }
    apply();
}

void ComposeUi::previewCurrent()
{
    m_speech.previewCurrent();
}

void ComposeUi::previewVoice(const QString& encodedId)
{
    const QString id = VoiceCatalog::decodeId(encodedId).trimmed();
    if (id.isEmpty()) {
        previewCurrent();
        return;
    }
    m_speech.previewVoice(id);
}

void ComposeUi::voicesPage(int delta)
{
    m_voicePage += delta;
    rebuildVoices();
}

void ComposeUi::voicesGoto(int offset)
{
    m_voicePage = offset;
    rebuildVoices();
}

void ComposeUi::setGenderFilter(const QString& gender)
{
    m_genderFilter = gender.trimmed().toLower();
    if (m_genderFilter == QLatin1String("all")) {
        m_genderFilter.clear();
    }
    m_voicePage = 0;
    rebuildVoices();
}

void ComposeUi::setLangFilter(const QString& language)
{
    QString lang = language.trimmed().toLower();
    if (lang == QLatin1String("all")) {
        lang.clear();
    }
    m_settings.speechLangFilter = lang;
    m_langBrowse = false;
    m_voicePage = 0;
    apply();
}

void ComposeUi::toggleLangBrowse()
{
    m_langBrowse = !m_langBrowse;
    if (m_langBrowse) {
        m_voiceSearch = false;
        m_voiceQuery.clear();
    }
    m_voicePage = 0;
    rebuildVoices();
}

void ComposeUi::toggleVoiceSearch()
{
    m_voiceSearch = !m_voiceSearch;
    m_langBrowse = false;
    if (!m_voiceSearch) {
        m_voiceQuery.clear();
    } else {
        showComposeLayer(1);
    }
    m_voicePage = 0;
    rebuildVoices();
}

void ComposeUi::exitVoiceSearch()
{
    if (!m_voiceSearch && m_voiceQuery.isEmpty()) {
        return;
    }
    m_voiceSearch = false;
    m_voiceQuery.clear();
    m_voicePage = 0;
    rebuildVoices();
}

bool ComposeUi::handleVoiceQuerySend(const PageAction& action)
{
    if (!m_voiceSearch || !hasLive(kVoicesLiveId)) {
        return false;
    }
    const QString key = action.sendKey.trimmed();
    QString add;
    if (key.size() == 1) {
        add = key;
    } else if (key.compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
        add = QStringLiteral(" ");
    }
    if (!add.isEmpty() && m_voiceQuery.size() < 40) {
        m_voiceQuery += add.left(40 - m_voiceQuery.size());
        m_voicePage = 0;
        rebuildVoices();
    }
    return true;
}

bool ComposeUi::handleVoiceQueryCommand(const QString& command)
{
    if (!m_voiceSearch || !hasLive(kVoicesLiveId)) {
        return false;
    }
    if (command == QLatin1String("backspace") || command == QLatin1String("compose.backspace")) {
        if (!m_voiceQuery.isEmpty()) {
            m_voiceQuery.chop(1);
            m_voicePage = 0;
            rebuildVoices();
        }
        return true;
    }
    if (command == QLatin1String("space")) {
        if (m_voiceQuery.size() < 40) {
            m_voiceQuery += QLatin1Char(' ');
            m_voicePage = 0;
            rebuildVoices();
        }
        return true;
    }
    if (command == QLatin1String("enter")) {
        exitVoiceSearch();
    }
    return true;
}

bool ComposeUi::handleVoicesEscape()
{
    if (m_voiceSearch && hasLive(kVoicesLiveId)) {
        exitVoiceSearch();
        return true;
    }
    const QString top = m_pages.topPageId();
    if (m_langBrowse && top == kVoicesLiveId) {
        m_langBrowse = false;
        m_voicePage = 0;
        rebuildVoices();
        return true;
    }
    if (top == kVoicesLiveId) {
        clearVoiceQuery();
        m_pages.closePage(QString(kVoicesLiveId));
        return true;
    }
    return false;
}

void ComposeUi::clearVoiceQuery()
{
    m_voiceSearch = false;
    m_voiceQuery.clear();
    m_langBrowse = false;
}

PageDocument ComposeUi::buildVoicesDocument()
{
    using VoiceCatalog::Voice;
    PageDocument doc;
    doc.id = QString(kVoicesLiveId);
    doc.name = QStringLiteral("Voices");
    const ThemeColors theme = m_settings.resolvedTheme();
    const QString height = m_voiceSearch ? QStringLiteral("A_ScreenHeight/12*5+8")
                                         : QStringLiteral("A_ScreenHeight");
    initTopOverlay(doc, 10, 3, {1.0, 1.0, 10.0}, 8, 16, theme, height, speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QString model = ElevenRequest::normalizeModelId(m_settings.speechModel);

    auto modelCell = [&](const QString& id, const QString& label, int col, int colSpan,
                         const QString& cmd, const QString& keyName) {
        PageCell c = cell(id, label, 0, col, cmd, model == keyName ? accent : key, colSpan);
        c.activeState = cmd;
        grid.cells.push_back(c);
    };
    modelCell(QStringLiteral("m_sapi"), QStringLiteral("SAPI"), 0, 1,
              QStringLiteral("speech.model.sapi"), QStringLiteral("sapi"));
    modelCell(QStringLiteral("m_turbo"), QStringLiteral("v4 Turbo"), 1, 2,
              QStringLiteral("speech.model.eleven_v4_turbo"),
              QStringLiteral("eleven_v4_turbo"));

    const QString current = currentVoiceId();
    grid.cells.push_back(cell(QStringLiteral("spd_dec"), QStringLiteral("\u2212"), 0, 3,
                              QStringLiteral("speech.speed.dec"), key));
    grid.cells.push_back(cell(QStringLiteral("spd_val"),
                              QString::number(m_settings.speechSpeed, 'f', 1), 0, 4, {}, value, 1,
                              QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("spd_inc"), QStringLiteral("+"), 0, 5,
                              QStringLiteral("speech.speed.inc"), key));
    grid.cells.push_back(cell(QStringLiteral("vol_dec"), QStringLiteral("\u2212"), 0, 6,
                              QStringLiteral("speech.volume.dec"), key));
    grid.cells.push_back(cell(QStringLiteral("vol_val"),
                              QString::number(m_settings.speechVolume, 'f', 1)
                                  + QStringLiteral("\u00d7"),
                              0, 7, {}, value, 1, QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("vol_inc"), QStringLiteral("+"), 0, 8,
                              QStringLiteral("speech.volume.inc"), key));
    PageCell done = cell(QStringLiteral("close"), QStringLiteral("Done"), 0, 9, {}, warn, 1);
    done.actions.push_back(closeSelf());
    grid.cells.push_back(done);

    const QString g = m_genderFilter;
    auto chip = [&](const QString& id, const QString& label, int col, const QString& cmd, bool on) {
        grid.cells.push_back(cell(id, label, 1, col, cmd, on ? accent : key));
    };
    chip(QStringLiteral("g_all"), QStringLiteral("All"), 0, QStringLiteral("speech.gender.all"),
         g.isEmpty());
    chip(QStringLiteral("g_f"), QStringLiteral("Female"), 1, QStringLiteral("speech.gender.female"),
         g == QLatin1String("female"));
    chip(QStringLiteral("g_m"), QStringLiteral("Male"), 2, QStringLiteral("speech.gender.male"),
         g == QLatin1String("male"));

    grid.cells.push_back(cell(QStringLiteral("pit_dec"), QStringLiteral("\u2212"), 1, 3,
                              QStringLiteral("speech.pitch.dec"), key));
    grid.cells.push_back(cell(QStringLiteral("pit_val"),
                              QString::number(m_settings.speechPitch, 'f', 1), 1, 4, {}, value, 1,
                              QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("pit_inc"), QStringLiteral("+"), 1, 5,
                              QStringLiteral("speech.pitch.inc"), key));
    const QString lang = m_settings.speechLangFilter;
    PageCell langBtn =
        cell(QStringLiteral("lang_browse"), lang.isEmpty() ? QStringLiteral("Language") : lang, 1,
             6, QStringLiteral("speech.lang.browse"), m_langBrowse ? accent : key, 2);
    grid.cells.push_back(std::move(langBtn));
    const QString query = m_voiceQuery.trimmed();
    PageCell search = cell(QStringLiteral("voice_search"),
                           query.isEmpty() ? QStringLiteral("Search") : query, 1, 8,
                           QStringLiteral("speech.voiceSearch"), m_voiceSearch ? accent : key, 2);
    grid.cells.push_back(std::move(search));

    if (m_langBrowse) {
        QStringList codes = VoiceCatalog::languages(currentVoices());
        if (!query.isEmpty()) {
            QStringList matched;
            for (const QString& code : codes) {
                if (code.contains(query, Qt::CaseInsensitive)) {
                    matched.push_back(code);
                }
            }
            codes = matched;
        }
        QStringList rows;
        if (query.isEmpty() || QLatin1String("all").contains(query, Qt::CaseInsensitive)) {
            rows.push_back(QString());
        }
        rows += codes;
        const int maxOffset = qMax(0, rows.size() - kOverlayListRows);
        m_voicePage = qBound(0, m_voicePage, maxOffset);
        const int start = m_voicePage;
        PageGrid body = overlayBody(2, 10);
        PageGrid list = overlayList(7);
        for (int i = 0; i < kOverlayListRows; ++i) {
            const int idx = start + i;
            list.cells.push_back(cell(QStringLiteral("f_%1").arg(i), {}, i, 0, {}, value, 1,
                                      QStringLiteral("label")));
            if (idx >= rows.size()) {
                list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), {}, i, 1, {}, value, 5,
                                          QStringLiteral("label")));
            } else if (rows.at(idx).isEmpty()) {
                list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), QStringLiteral("All"), i,
                                          1, QStringLiteral("speech.lang.all"),
                                          lang.isEmpty() ? accent : key, 5));
            } else {
                const QString code = rows.at(idx);
                list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), code, i, 1,
                                          QStringLiteral("speech.lang.set.%1").arg(code),
                                          lang == code ? accent : key, 5));
            }
            list.cells.push_back(cell(QStringLiteral("p_%1").arg(i), {}, i, 6, {}, value, 1,
                                      QStringLiteral("label")));
        }
        body.subGrids.push_back(std::move(list));
        body.subGrids.push_back(overlayScrollbar(start, kOverlayListRows, rows.size(), key,
                                                QStringLiteral("speech.voiceList.prev"),
                                                QStringLiteral("speech.voiceList.next")));
        grid.subGrids.push_back(std::move(body));
        return doc;
    }

    const auto filtered = filteredVoices();
    const int maxOffset = qMax(0, filtered.size() - kOverlayListRows);
    m_voicePage = qBound(0, m_voicePage, maxOffset);
    const int start = m_voicePage;
    QString caption;
    if (m_voicesLoading) {
        caption = QStringLiteral("Loading voices\u2026");
    } else if (elevenMode() && !m_secrets.hasKey()) {
        caption = QStringLiteral("Set an ElevenLabs API key in Settings");
    } else if (filtered.isEmpty()) {
        caption = elevenMode() ? QStringLiteral("No matching ElevenLabs voices")
                               : QStringLiteral("No matching SAPI voices");
    } else {
        caption = QStringLiteral("%1–%2 of %3")
                      .arg(start + 1)
                      .arg(qMin(start + kOverlayListRows, filtered.size()))
                      .arg(filtered.size());
    }
    for (PageCell& c : grid.cells) {
        if (c.id == QLatin1String("lang_browse")) {
            c.caption = caption;
            break;
        }
    }

    PageGrid body = overlayBody(2, 10);
    PageGrid list = overlayList(7);
    for (int i = 0; i < kOverlayListRows; ++i) {
        const int idx = start + i;
        if (idx >= filtered.size()) {
            list.cells.push_back(cell(QStringLiteral("f_%1").arg(i), {}, i, 0, {}, value, 1,
                                      QStringLiteral("label")));
            list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), {}, i, 1, {}, value, 5,
                                      QStringLiteral("label")));
            list.cells.push_back(cell(QStringLiteral("p_%1").arg(i), {}, i, 6, {}, value, 1,
                                      QStringLiteral("label")));
            continue;
        }
        const Voice& v = filtered[idx];
        const QString encoded = VoiceCatalog::encodeId(v.id);
        const bool sel = !current.isEmpty() && v.id == current;
        if (elevenMode()) {
            PageCell fav = cell(QStringLiteral("f_%1").arg(i), {}, i, 0,
                                QStringLiteral("speech.fav.toggle.%1").arg(encoded), key, 1, {}, {},
                                QStringLiteral("star"));
            fav.activeState = QStringLiteral("speech.fav.%1").arg(encoded);
            list.cells.push_back(fav);
        } else {
            list.cells.push_back(cell(QStringLiteral("f_%1").arg(i), {}, i, 0, {}, value, 1,
                                      QStringLiteral("label")));
        }
        if (sel) {
            PageCell name = cell(QStringLiteral("v_%1").arg(i), v.name, i, 1, {}, accent, 5,
                                 QStringLiteral("label"), v.language);
            name.activeState = QStringLiteral("speech.voice.%1").arg(encoded);
            list.cells.push_back(name);
        } else {
            list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), v.name, i, 1,
                                      QStringLiteral("speech.voice.%1").arg(encoded), key, 5, {},
                                      v.language));
        }
        list.cells.push_back(cell(QStringLiteral("p_%1").arg(i), {}, i, 6,
                                  QStringLiteral("speech.voicePreview.%1").arg(encoded), accent, 1,
                                  {}, {}, QStringLiteral("recordVoiceOver")));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kOverlayListRows, filtered.size(), key,
                                            QStringLiteral("speech.voiceList.prev"),
                                            QStringLiteral("speech.voiceList.next")));
    grid.subGrids.push_back(std::move(body));
    return doc;
}

} // namespace gazer
