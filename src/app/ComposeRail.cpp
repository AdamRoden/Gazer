#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "ui/Theme.h"

#include <QUrl>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using SettingsPageBuild::makeNested;
using compose_detail::aboveKeyboardHeight;
using compose_detail::commandAction;
using compose_detail::kTileCols;
using compose_detail::kTilePage;
using compose_detail::kTileRows;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;
using compose_detail::paintSurface;
using compose_detail::railActionCatalog;
using compose_detail::railActionLookup;
using compose_detail::searchField;
using compose_detail::stampEditableGrid;
using compose_detail::trackPx;

namespace {

QString percent(const QString& text)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(text.toUtf8()));
}

bool sameFavorite(const AppSettings::RailFavorite& a, const AppSettings::RailFavorite& b)
{
    if (a.kind != b.kind) {
        return false;
    }
    if (a.kind == QLatin1String("style")) {
        return VoiceStyles::keyOf(a.ref) == VoiceStyles::keyOf(b.ref);
    }
    if (a.kind == QLatin1String("tag")) {
        return a.ref.compare(b.ref, Qt::CaseInsensitive) == 0;
    }
    return a.ref == b.ref;
}

/// Label, icon, and the command a favorite runs once it is on the rail.
struct RailFace {
    QString label;
    QString icon;
    QString caption;
    QString command;
};

RailFace railFace(const QString& kind, const QString& ref)
{
    RailFace face;
    if (kind == QLatin1String("style")) {
        face.label = ref;
        face.icon = QStringLiteral("palette");
        face.caption = QStringLiteral("Style");
        face.command = QStringLiteral("compose.applyStyle.%1").arg(percent(ref));
        return face;
    }
    if (kind == QLatin1String("tag")) {
        face.label = ref;
        face.icon = QStringLiteral("sell");
        face.caption = QStringLiteral("Tag");
        face.command = QStringLiteral("compose.insertTag.%1").arg(percent(ref));
        return face;
    }
    const compose_detail::RailActionSpec spec = railActionLookup(ref);
    face.label = spec.label;
    face.icon = spec.icon;
    face.caption = QStringLiteral("Action");
    face.command = spec.command;
    return face;
}

} // namespace

QVector<AppSettings::RailFavorite> ComposeUi::railItems() const
{
    if (m_settings.railFavoritesSet) {
        return m_settings.railFavorites;
    }
    QVector<AppSettings::RailFavorite> items;
    for (const QString& command : compose_detail::defaultRailCommands()) {
        AppSettings::RailFavorite item;
        item.kind = QStringLiteral("action");
        item.ref = command;
        items.push_back(item);
    }
    return items;
}

void ComposeUi::storeRail(const QVector<AppSettings::RailFavorite>& items)
{
    m_settings.railFavorites = items;
    m_settings.railFavoritesSet = true;
    apply();
}

void ComposeUi::fillRail(PageDocument& doc) const
{
    PageGrid* grid = doc.findGrid(QStringLiteral("rail"));
    if (!grid) {
        return;
    }
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const bool editing = editMode();
    const QVector<AppSettings::RailFavorite> items = railItems();
    const int itemSlots = qMin(items.size(), 8);
    stampEditableGrid(grid, editing, accent);
    grid->cells.clear();
    grid->rows = 8;
    grid->columns = 1;
    int slot = 0;
    for (int i = 0; i < itemSlots; ++i) {
        const AppSettings::RailFavorite& item = items.at(i);
        const RailFace face = railFace(item.kind, item.ref);
        PageCell button;
        button.id = QStringLiteral("fav_%1").arg(i);
        button.row = slot++;
        button.col = 0;
        button.label = face.label;
        button.icon = face.icon;
        button.style.background = surface;
        button.style.foreground = ThemeColors::contrastOn(surface);
        if (!editing && !face.command.isEmpty()) {
            button.actions.push_back(commandAction(face.command));
            if (item.kind == QLatin1String("style")) {
                button.activeState = QStringLiteral("compose.style.%1")
                                         .arg(percent(VoiceStyles::keyOf(item.ref)));
            }
        }
        if (editing) {
            button.icon = QStringLiteral("delete");
            button.actions.clear();
            button.actions.push_back(
                commandAction(QStringLiteral("compose.railRemove.%1").arg(i)));
            button.activeState.clear();
        }
        grid->cells.push_back(std::move(button));
    }
    for (; slot < 8; ++slot) {
        PageCell empty;
        empty.id = QStringLiteral("fav_empty_%1").arg(slot);
        empty.row = slot;
        empty.col = 0;
        empty.style.background = value;
        if (editing && items.size() < 8) {
            empty.icon = QStringLiteral("add");
            empty.actions.push_back(commandAction(QStringLiteral("compose.openRailAdd")));
        } else {
            empty.role = QStringLiteral("label");
        }
        grid->cells.push_back(std::move(empty));
    }
}

void ComposeUi::toggleRail()
{
    m_showFavorites = !m_showFavorites;
    rebuildBoard();
}

bool ComposeUi::openRailAdd(QString* error)
{
    m_railSearch = true;
    m_railQuery.clear();
    m_railPage = 0;
    const bool ok = presentLive(QString(kRailLiveId), buildRailAddDocument(), error);
    if (ok && isOpen()) {
        showComposeLayer(1);
    }
    return ok;
}

void ComposeUi::rebuildRailAdd()
{
    if (!hasLive(kRailLiveId)) {
        return;
    }
    QString err;
    PageDocument doc = buildRailAddDocument();
    doc.id = QString(kRailLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

void ComposeUi::addRailFavorite(const QString& spec)
{
    const int dot = spec.indexOf(QLatin1Char('.'));
    if (dot <= 0) {
        return;
    }
    const QString kind = spec.left(dot);
    const QString rest = spec.mid(dot + 1);
    AppSettings::RailFavorite item;
    if (kind == QLatin1String("action")) {
        if (rest.isEmpty()) {
            return;
        }
        item.kind = QStringLiteral("action");
        item.ref = rest;
    } else if (kind == QLatin1String("style")) {
        item.kind = QStringLiteral("style");
        item.ref = VoiceStyles::normalizeStyleName(QUrl::fromPercentEncoding(rest.toUtf8()));
        if (item.ref.isEmpty()) {
            return;
        }
    } else if (kind == QLatin1String("tag")) {
        item.kind = QStringLiteral("tag");
        item.ref = AppSettings::normalizeSpeechTag(QUrl::fromPercentEncoding(rest.toUtf8()));
        if (item.ref.isEmpty()) {
            return;
        }
    } else {
        return;
    }
    QVector<AppSettings::RailFavorite> items = railItems();
    for (const AppSettings::RailFavorite& have : items) {
        if (sameFavorite(have, item)) {
            return;
        }
    }
    if (items.size() >= 8) {
        notify(QStringLiteral("Favorites are full"));
        return;
    }
    items.push_back(item);
    if (hasLive(kRailLiveId)) {
        m_pages.closePage(QString(kRailLiveId));
    }
    storeRail(items);
}

void ComposeUi::removeRailFavorite(int index)
{
    QVector<AppSettings::RailFavorite> items = railItems();
    if (index < 0 || index >= items.size()) {
        return;
    }
    items.removeAt(index);
    storeRail(items);
}

void ComposeUi::railGoto(int offset)
{
    m_railPage = offset;
    rebuildRailAdd();
}

void ComposeUi::railPage(int delta)
{
    if (delta == 0 || !hasLive(kRailLiveId)) {
        return;
    }
    m_railPage += delta * kTileCols;
    rebuildRailAdd();
}

PageDocument ComposeUi::buildRailAddDocument()
{
    PageDocument doc;
    doc.id = QString(kRailLiveId);
    doc.name = QStringLiteral("Favorites");
    const ThemeColors theme = m_settings.resolvedTheme();
    initTopOverlay(doc, 1, 3, {1, 1, 1}, 8, 16, theme, aboveKeyboardHeight(m_composeAuthored),
                   speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {trackPx(72), trackPx(72), PageTrackSize::starWeight()};
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);

    grid.subGrids.push_back(compose_detail::speakTitleBar(QStringLiteral("Add favorite"), false, key));
    grid.cells.push_back(searchField(QStringLiteral("rail_search"), m_railQuery,
                                     QStringLiteral("Search"), 1, 0, 1, key, accent));

    struct Entry {
        QString label;
        QString caption;
        QString icon;
        QString command;
    };
    const QVector<AppSettings::RailFavorite> have = railItems();
    auto taken = [&](const QString& kind, const QString& ref) {
        AppSettings::RailFavorite probe;
        probe.kind = kind;
        probe.ref = ref;
        for (const AppSettings::RailFavorite& item : have) {
            if (sameFavorite(item, probe)) {
                return true;
            }
        }
        return false;
    };
    QVector<Entry> entries;
    const QString query = m_railQuery.trimmed();
    auto consider = [&](const QString& kind, const QString& ref, const QString& payload) {
        const RailFace face = railFace(kind, ref);
        if (taken(kind, ref) || face.label.isEmpty()) {
            return;
        }
        if (!query.isEmpty() && !face.label.contains(query, Qt::CaseInsensitive)) {
            return;
        }
        entries.push_back(Entry{face.label, face.caption, face.icon,
                                QStringLiteral("compose.railAdd.%1.%2").arg(kind, payload)});
    };
    for (const compose_detail::RailActionSpec& spec : railActionCatalog()) {
        consider(QStringLiteral("action"), spec.command, spec.command);
    }
    for (const QString& name : styleCatalog().styles) {
        consider(QStringLiteral("style"), name, percent(name));
    }
    for (const AppSettings::SavedSpeechTag& tag : m_settings.savedSpeechTags) {
        const QString name = AppSettings::normalizeSpeechTag(tag.name);
        if (!name.isEmpty()) {
            consider(QStringLiteral("tag"), name, percent(name));
        }
    }
    const int maxOff = qMax(0, entries.size() - kTilePage);
    m_railPage = qBound(0, m_railPage, maxOff);
    const int start = m_railPage;

    PageGrid body = overlayBody(2, 1);
    PageGrid list = overlayList(kTileCols, kTileRows);
    list.gapPx = 8;
    for (int slot = 0; slot < kTilePage; ++slot) {
        const int index = start + slot;
        const int tileRow = slot / kTileCols;
        const int tileCol = slot % kTileCols;
        if (index >= entries.size()) {
            list.cells.push_back(cell(QStringLiteral("rail_add_%1").arg(slot), {}, tileRow, tileCol,
                                      {}, value, 1, QStringLiteral("label")));
            continue;
        }
        const Entry& entry = entries.at(index);
        PageCell tile = cell(QStringLiteral("rail_add_%1").arg(slot), entry.label, tileRow, tileCol,
                             entry.command, key, 1, {}, entry.caption, entry.icon);
        paintSurface(tile, key);
        list.cells.push_back(std::move(tile));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kTilePage, entries.size(), key,
                                            QStringLiteral("compose.railPage.-1"),
                                            QStringLiteral("compose.railPage.1")));
    grid.subGrids.push_back(std::move(body));
    return doc;
}

} // namespace gazer
