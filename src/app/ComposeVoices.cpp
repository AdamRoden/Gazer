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
    if (freestyleMode()) {
        rebuildBoard();
    }
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
    m_voicePage = 0;
    apply();
}

PageDocument ComposeUi::buildVoicesDocument()
{
    using VoiceCatalog::Voice;
    PageDocument doc;
    doc.id = QString(kVoicesLiveId);
    doc.name = QStringLiteral("Voices");
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 10, 3, {1.0, 1.0, 10.0}, 8, 16, theme, QStringLiteral("A_ScreenHeight"),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    const QColor key = theme.bgSurface.isValid() ? theme.bgSurface : QColor(40, 40, 44);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const QString model = ElevenRequest::normalizeModelId(m_settings.speechModel);

    auto modelCell = [&](const QString& id, const QString& label, int col, const QString& cmd,
                         const QString& keyName) {
        PageCell c = cell(id, label, 0, col, cmd, model == keyName ? accent : key);
        c.activeState = cmd;
        grid.cells.push_back(c);
    };
    modelCell(QStringLiteral("m_sapi"), QStringLiteral("SAPI"), 0,
              QStringLiteral("speech.model.sapi"), QStringLiteral("sapi"));
    modelCell(QStringLiteral("m_flash"), QStringLiteral("Flash"), 1,
              QStringLiteral("speech.model.eleven_flash_v2_5"),
              QStringLiteral("eleven_flash_v2_5"));
    modelCell(QStringLiteral("m_v3"), QStringLiteral("v3"), 2,
              QStringLiteral("speech.model.eleven_v3"), QStringLiteral("eleven_v3"));

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

    const QString lang = m_settings.speechLangFilter;
    chip(QStringLiteral("l_all"), QStringLiteral("All lang"), 3, QStringLiteral("speech.lang.all"),
         lang.isEmpty());
    const QStringList langs = VoiceCatalog::languageChips(currentVoices(), VoiceCatalog::kLangChips);
    for (int i = 0; i < VoiceCatalog::kLangChips; ++i) {
        if (i >= langs.size()) {
            grid.cells.push_back(cell(QStringLiteral("l_%1").arg(i), {}, 1, 4 + i, {}, value, 1,
                                      QStringLiteral("label")));
            continue;
        }
        const QString code = langs[i];
        chip(QStringLiteral("l_%1").arg(i), code, 4 + i,
             QStringLiteral("speech.lang.set.%1").arg(code), lang == code);
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
    grid.cells.push_back(cell(QStringLiteral("caption"), caption, 1, 6, {}, value, 4,
                              QStringLiteral("value")));

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
    body.subGrids.push_back(overlayScrollbar(start, kOverlayListRows, filtered.size(), key));
    grid.subGrids.push_back(std::move(body));
    return doc;
}

} // namespace gazer
