#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechSecrets.h"
#include "assist/TtsService.h"
#include "layout/PageDim.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QColor>
#include <QJsonObject>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using compose_detail::aboveKeyboardHeight;
using compose_detail::kSearchListRows;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;
using compose_detail::paintFlat;
using compose_detail::paintSelected;
using compose_detail::paintSurface;
using compose_detail::searchField;
using compose_detail::trackPx;
using SettingsPageBuild::makeNested;

bool ComposeUi::openVoices(QString* error)
{
    m_voicePage = 0;
    m_voiceSearch = true;
    m_voiceQuery.clear();
    m_langBrowse = false;
    requestCatalogIfNeeded();
    const bool ok = presentLive(QString(kVoicesLiveId), buildVoicesDocument(), error);
    if (ok && isOpen()) {
        showComposeLayer(1);
    }
    return ok;
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
    PageDocument doc = buildVoicesDocument();
    doc.id = QString(kVoicesLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
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
    m_voiceSearch = true;
    m_voicePage = 0;
    rebuildVoices();
}

void ComposeUi::toggleVoiceSearch()
{
    if (!hasLive(kVoicesLiveId)) {
        QString err;
        openVoices(&err);
        return;
    }
    m_voiceSearch = true;
    if (isOpen()) {
        showComposeLayer(1);
    }
    rebuildVoices();
}

void ComposeUi::exitVoiceSearch()
{
    if (m_voiceQuery.isEmpty() && !m_langBrowse) {
        return;
    }
    m_voiceQuery.clear();
    m_langBrowse = false;
    m_voiceSearch = true;
    m_voicePage = 0;
    rebuildVoices();
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
    initTopOverlay(doc, 1, 5, {1, 1, 1, 1, 1}, 8, 16, theme,
                   aboveKeyboardHeight(m_composeAuthored), speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {trackPx(64), trackPx(72), trackPx(72), trackPx(72),
                      PageTrackSize::starWeight()};
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QString model = ElevenRequest::normalizeModelId(m_settings.speechModel);
    const QString current = currentVoiceId();
    const QString lang = m_settings.speechLangFilter;
    const QString g = m_genderFilter;
    const bool windows = model == QLatin1String("sapi");

    PageGrid title = compose_detail::speakTitleBar(QStringLiteral("Select Voice"), false, key);
    if (!title.cells.isEmpty()) {
        title.cells[0].caption = QStringLiteral("Voice model");
    }
    grid.subGrids.push_back(std::move(title));

    PageGrid models = makeNested(QStringLiteral("models"), 1, 0, 1, 2, 8);
    models.columnTracks = {PageTrackSize::starWeight(), PageTrackSize::starWeight()};
    auto modelCell = [&](const QString& id, const QString& label, int col, const QString& cmd,
                         bool on) {
        PageCell c = cell(id, label, 0, col, cmd, on ? key : value);
        c.activeState = cmd;
        c.style.radius = PageBox::all(12);
        if (on) {
            paintSelected(c, accent, key);
        }
        models.cells.push_back(std::move(c));
    };
    modelCell(QStringLiteral("m_turbo"), QStringLiteral("ElevenLabs v4 Turbo"), 0,
              QStringLiteral("speech.model.eleven_v4_turbo"), !windows);
    modelCell(QStringLiteral("m_sapi"), QStringLiteral("Windows"), 1, QStringLiteral("speech.model.sapi"),
              windows);
    grid.subGrids.push_back(std::move(models));

    grid.cells.push_back(searchField(QStringLiteral("voice_search"), m_voiceQuery,
                                     QStringLiteral("Search voices"), 2, 0, 1, key, accent));

    QStringList langs = VoiceCatalog::languageChips(currentVoices(), 3);
    if (!lang.isEmpty() && !langs.contains(lang)) {
        if (langs.isEmpty()) {
            langs.push_back(lang);
        } else {
            langs.last() = lang;
        }
    }
    PageGrid filters = makeNested(QStringLiteral("filters"), 3, 0, 1, 9, 8);
    for (int i = 0; i < 5; ++i) {
        filters.columnTracks.push_back(PageTrackSize::starWeight());
    }
    filters.columnTracks.push_back(PageTrackSize::starWeight(0.6));
    for (int i = 0; i < 3; ++i) {
        filters.columnTracks.push_back(trackPx(120));
    }
    auto chip = [&](const QString& id, const QString& label, int col, const QString& cmd, bool on) {
        PageCell c = cell(id, label, 0, col, cmd, on ? key : value);
        c.style.radius = PageBox::all(1000);
        if (on) {
            paintSelected(c, accent, key);
        }
        filters.cells.push_back(std::move(c));
    };
    for (int i = 0; i < 3; ++i) {
        if (i < langs.size()) {
            const QString code = langs.at(i);
            chip(QStringLiteral("lang_%1").arg(i), code, i,
                 QStringLiteral("speech.lang.set.%1").arg(code), lang == code && !m_langBrowse);
        } else {
            filters.cells.push_back(cell(QStringLiteral("lang_%1").arg(i), {}, 0, i, {}, value, 1,
                                         QStringLiteral("label")));
        }
    }
    chip(QStringLiteral("lang_all"), QStringLiteral("All languages"), 3,
         QStringLiteral("speech.lang.all"), lang.isEmpty() && !m_langBrowse);
    chip(QStringLiteral("lang_more"), QStringLiteral("More"), 4, QStringLiteral("speech.lang.browse"),
         m_langBrowse);
    chip(QStringLiteral("g_all"), QStringLiteral("All"), 6, QStringLiteral("speech.gender.all"),
         g.isEmpty());
    chip(QStringLiteral("g_f"), QStringLiteral("Female"), 7, QStringLiteral("speech.gender.female"),
         g == QLatin1String("female"));
    chip(QStringLiteral("g_m"), QStringLiteral("Male"), 8, QStringLiteral("speech.gender.male"),
         g == QLatin1String("male"));
    grid.subGrids.push_back(std::move(filters));

    const QString query = m_voiceQuery.trimmed();
    auto putCaption = [&](const QString& caption) {
        for (PageGrid& sub : grid.subGrids) {
            if (sub.id == QLatin1String("titlebar") && !sub.cells.isEmpty()) {
                sub.cells[0].caption = caption;
                return;
            }
        }
    };

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
        const int maxOffset = qMax(0, rows.size() - kSearchListRows);
        m_voicePage = qBound(0, m_voicePage, maxOffset);
        const int start = m_voicePage;
        putCaption(rows.isEmpty() ? QStringLiteral("No matching languages")
                                  : QStringLiteral("Languages"));
        PageGrid body = overlayBody(4, 1);
        PageGrid list = overlayList(1, kSearchListRows);
        for (int i = 0; i < kSearchListRows; ++i) {
            const int idx = start + i;
            if (idx >= rows.size()) {
                list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), {}, i, 0, {}, value, 1,
                                          QStringLiteral("label")));
            } else if (rows.at(idx).isEmpty()) {
                PageCell all = cell(QStringLiteral("v_%1").arg(i), QStringLiteral("All languages"), i,
                                    0, QStringLiteral("speech.lang.all"),
                                    lang.isEmpty() ? accent : key);
                list.cells.push_back(std::move(all));
            } else {
                const QString code = rows.at(idx);
                list.cells.push_back(cell(QStringLiteral("v_%1").arg(i), code, i, 0,
                                          QStringLiteral("speech.lang.set.%1").arg(code),
                                          lang == code ? accent : key));
            }
        }
        body.subGrids.push_back(std::move(list));
        body.subGrids.push_back(overlayScrollbar(start, kSearchListRows, rows.size(), key,
                                                QStringLiteral("speech.voiceList.prev"),
                                                QStringLiteral("speech.voiceList.next")));
        grid.subGrids.push_back(std::move(body));
        return doc;
    }

    const auto filtered = filteredVoices();
    const int maxOffset = qMax(0, filtered.size() - kSearchListRows);
    m_voicePage = qBound(0, m_voicePage, maxOffset);
    const int start = m_voicePage;
    QString caption;
    if (m_voicesLoading) {
        caption = QStringLiteral("Loading voices\u2026");
    } else if (elevenMode() && !m_secrets.hasKey()) {
        caption = QStringLiteral("Set an ElevenLabs API key in Settings");
    } else if (filtered.isEmpty()) {
        caption = elevenMode() ? QStringLiteral("No matching ElevenLabs voices")
                               : QStringLiteral("No matching Windows voices");
    }
    if (!caption.isEmpty()) {
        putCaption(caption);
    }

    PageGrid body = overlayBody(4, 1);
    PageGrid list = overlayList(1, kSearchListRows);
    list.gapPx = 8;
    for (int i = 0; i < kSearchListRows; ++i) {
        const int idx = start + i;
        if (idx >= filtered.size()) {
            PageCell empty = cell(QStringLiteral("voice_row_%1").arg(i), {}, i, 0, {}, QColor(), 1,
                                  QStringLiteral("label"));
            paintFlat(empty);
            list.cells.push_back(std::move(empty));
            continue;
        }
        const bool eleven = elevenMode();
        PageGrid row = makeNested(QStringLiteral("voice_row_%1").arg(i), i, 0, 1, eleven ? 3 : 2, 8);
        row.marginPx = 6;
        if (eleven) {
            row.columnTracks = {trackPx(64), PageTrackSize::starWeight(), trackPx(104)};
        } else {
            row.columnTracks = {PageTrackSize::starWeight(), trackPx(104)};
        }
        row.style.radius = PageBox::all(12);
        const Voice& v = filtered[idx];
        const QString encoded = VoiceCatalog::encodeId(v.id);
        const bool sel = !current.isEmpty() && v.id == current;
        QString gender = v.gender.trimmed();
        if (!gender.isEmpty()) {
            gender[0] = gender[0].toUpper();
        }
        QString meta = v.language;
        if (!gender.isEmpty()) {
            meta = meta.isEmpty() ? gender : QStringLiteral("%1 · %2").arg(v.language, gender);
        }
        const QColor rim = ThemeColors::mix(value, ThemeColors::contrastOn(value), 0.28);
        row.style.background = value;
        row.style.borderColor = sel ? accent : rim;
        row.style.thickness = PageBox::all(sel ? 2 : 1);
        const int nameCol = eleven ? 1 : 0;
        const int playCol = eleven ? 2 : 1;
        if (eleven) {
            PageCell fav = cell(QStringLiteral("f_%1").arg(i), {}, 0, 0,
                                QStringLiteral("speech.fav.toggle.%1").arg(encoded), key, 1, {}, {},
                                QStringLiteral("star"));
            fav.activeState = QStringLiteral("speech.fav.%1").arg(encoded);
            paintSurface(fav, key);
            row.cells.push_back(std::move(fav));
        }
        PageCell name = cell(QStringLiteral("v_%1").arg(i), v.name, 0, nameCol,
                             QStringLiteral("speech.voice.%1").arg(encoded), QColor());
        name.caption = meta;
        name.textStyle = QStringLiteral("row");
        paintFlat(name);
        row.cells.push_back(std::move(name));
        PageCell play = cell(QStringLiteral("p_%1").arg(i), {}, 0, playCol,
                             QStringLiteral("speech.voicePreview.%1").arg(encoded), key, 1, {}, {},
                             QStringLiteral("Speak"));
        paintSurface(play, key);
        row.cells.push_back(std::move(play));
        list.subGrids.push_back(std::move(row));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kSearchListRows, filtered.size(), key,
                                            QStringLiteral("speech.voiceList.prev"),
                                            QStringLiteral("speech.voiceList.next")));
    grid.subGrids.push_back(std::move(body));
    return doc;
}

} // namespace gazer
