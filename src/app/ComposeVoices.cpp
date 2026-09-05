#include "app/ComposeUi.h"

#include "app/AppSettings.h"
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
using SettingsPageBuild::initGrid;

bool ComposeUi::openVoices(QString* error)
{
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

void ComposeUi::toggleFavorite()
{
    if (!elevenMode()) {
        return;
    }
    const QString id = m_settings.elevenVoiceId.trimmed();
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

void ComposeUi::voicesPage(int delta)
{
    m_voicePage += delta;
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
    initGrid(doc, 6, 6, 960, 720, 10, 16, theme);
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
    grid.cells.push_back(cell(QStringLiteral("preview"), QStringLiteral("Preview"), 0, 3,
                              QStringLiteral("speech.preview"), accent));
    grid.cells.push_back(cell(QStringLiteral("prev"), QStringLiteral("Prev"), 0, 4,
                              QStringLiteral("speech.voiceList.prev"), key));
    grid.cells.push_back(cell(QStringLiteral("next"), QStringLiteral("Next"), 0, 5,
                              QStringLiteral("speech.voiceList.next"), key));

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
    int pages = 1;
    const auto vis = VoiceCatalog::page(filtered, m_voicePage, &pages);
    m_voicePage = qBound(0, m_voicePage, pages - 1);
    QString caption;
    if (m_voicesLoading) {
        caption = QStringLiteral("Loading voices\u2026");
    } else if (elevenMode() && !m_secrets.hasKey()) {
        caption = QStringLiteral("Set an ElevenLabs API key in Settings");
    } else if (filtered.isEmpty()) {
        caption = elevenMode() ? QStringLiteral("No matching ElevenLabs voices")
                               : QStringLiteral("No matching SAPI voices");
    } else {
        const int start = m_voicePage * VoiceCatalog::kPageSize + 1;
        const int end = start + vis.size() - 1;
        caption = QStringLiteral("%1–%2 of %3")
                      .arg(start)
                      .arg(end)
                      .arg(filtered.size());
    }
    grid.cells.push_back(cell(QStringLiteral("caption"), caption, 2, 0, {}, value, 5,
                              QStringLiteral("value")));
    PageCell done = cell(QStringLiteral("close"), QStringLiteral("Done"), 2, 5, {}, warn);
    done.actions.push_back(closeSelf());
    grid.cells.push_back(done);

    const QString current = currentVoiceId();
    const QStringList& favs = m_settings.elevenFavoriteVoiceIds;
    for (int i = 0; i < VoiceCatalog::kPageSize; ++i) {
        const int row = 3 + i / 6;
        const int col = i % 6;
        if (i >= vis.size()) {
            grid.cells.push_back(cell(QStringLiteral("v_%1").arg(i), {}, row, col, {}, value, 1,
                                      QStringLiteral("label")));
            continue;
        }
        const Voice& v = vis[i];
        const bool sel = v.id == current;
        const bool fav = elevenMode() && favs.contains(v.id);
        QString label = v.name;
        if (fav) {
            label = QStringLiteral("\u2605 ") + label;
        }
        PageCell c = cell(QStringLiteral("v_%1").arg(i), label, row, col,
                          QStringLiteral("speech.voice.%1").arg(VoiceCatalog::encodeId(v.id)),
                          sel ? accent : key, 1, {}, v.language);
        grid.cells.push_back(c);
    }

    if (elevenMode() && !current.isEmpty()) {
        grid.cells.push_back(cell(QStringLiteral("star"), QStringLiteral("\u2605 Favorite"), 5, 0,
                                  QStringLiteral("speech.fav.toggle"),
                                  favs.contains(current) ? accent : key, 2));
    } else {
        grid.cells.push_back(cell(QStringLiteral("star"),
                                  elevenMode() ? QStringLiteral("Select a voice")
                                               : QStringLiteral("SAPI voice"),
                                  5, 0, {}, value, 2, QStringLiteral("label")));
    }
    grid.cells.push_back(cell(QStringLiteral("spd_dec"), QStringLiteral("\u2212"), 5, 2,
                              QStringLiteral("speech.speed.dec"), key));
    grid.cells.push_back(cell(QStringLiteral("spd_val"),
                              QString::number(m_settings.speechSpeed, 'f', 1), 5, 3, {}, value, 1,
                              QStringLiteral("value")));
    grid.cells.push_back(cell(QStringLiteral("spd_inc"), QStringLiteral("+"), 5, 4,
                              QStringLiteral("speech.speed.inc"), key));
    return doc;
}

} // namespace gazer
