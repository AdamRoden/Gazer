#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QUrl>
#include <QVector>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using SettingsPageBuild::makeNested;
using compose_detail::aboveKeyboardHeight;
using compose_detail::kTileCols;
using compose_detail::kTilePage;
using compose_detail::kTileRows;
using compose_detail::paintDanger;
using compose_detail::paintSave;
using compose_detail::paintSelected;
using compose_detail::paintSurface;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;
using compose_detail::searchField;
using compose_detail::trackPx;

namespace {

QString encodeStyleName(const QString& name)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(name.toUtf8()));
}

} // namespace

void ComposeUi::applyStyle(const QString& encoded)
{
    const QString raw = QUrl::fromPercentEncoding(encoded.toUtf8());
    const QString name = VoiceStyles::normalizeStyleName(raw);
    if (name.isEmpty()) {
        return;
    }
    m_settings.speechStyle = name;
    apply();
}

void ComposeUi::clearStyle()
{
    if (m_settings.speechStyle.isEmpty()) {
        return;
    }
    m_settings.speechStyle.clear();
    apply();
}

VoiceStyles::Catalog ComposeUi::styleCatalog() const
{
    if (!m_settings.voiceStylesSet) {
        return VoiceStyles::defaultCatalog();
    }
    VoiceStyles::Catalog catalog;
    catalog.styles = m_settings.voiceStyleNames;
    catalog.favorites = m_settings.voiceStyleFavorites;
    return catalog;
}

void ComposeUi::storeStyleCatalog(const VoiceStyles::Catalog& catalog)
{
    m_settings.voiceStyleNames = catalog.styles;
    m_settings.voiceStyleFavorites = catalog.favorites;
    m_settings.voiceStylesSet = true;
    apply();
}

void ComposeUi::showComposeLayer(int layer)
{
    PageAction action;
    action.type = PageActionType::ShowLayers;
    action.layers = {layer};
    QString err;
    (void)m_pages.showLayers({action}, QString(kPageId), {}, &err);
}

void ComposeUi::openStyleEditor()
{
    m_styleEditor = true;
    m_stylePage = 0;
    m_styleSearch = true;
    if (!hasLive(kStylesLiveId)) {
        if (isOpen()) {
            showComposeLayer(1);
        }
        QString err;
        (void)presentLive(QString(kStylesLiveId), buildStylesDocument(), &err);
        return;
    }
    rebuildStyles();
}

void ComposeUi::closeStyleEditor()
{
    m_styleEditor = false;
    m_stylePage = 0;
    rebuildStyles();
}

void ComposeUi::pageStyles(int delta)
{
    if (delta == 0 || !hasLive(kStylesLiveId)) {
        return;
    }
    m_stylePage += delta * kTileCols;
    rebuildStyles();
}

void ComposeUi::stylesGoto(int offset)
{
    m_stylePage = offset;
    rebuildStyles();
}

void ComposeUi::toggleStyleFavorite(const QString& encoded)
{
    const QString name = VoiceStyles::normalizeStyleName(
        QUrl::fromPercentEncoding(encoded.toUtf8()));
    if (name.isEmpty()) {
        return;
    }
    const VoiceStyles::Catalog current = styleCatalog();
    const QString key = VoiceStyles::keyOf(name);
    bool found = false;
    for (const QString& style : current.styles) {
        if (VoiceStyles::keyOf(style) == key) {
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    storeStyleCatalog(VoiceStyles::toggleFavorite(current, name));
}

void ComposeUi::trashStyle(const QString& encoded)
{
    const QString name = VoiceStyles::normalizeStyleName(
        QUrl::fromPercentEncoding(encoded.toUtf8()));
    if (name.isEmpty()) {
        return;
    }
    const VoiceStyles::Result result = VoiceStyles::trashStyle(styleCatalog(), name);
    if (!result.ok) {
        return;
    }
    if (VoiceStyles::keyOf(m_settings.speechStyle) == VoiceStyles::keyOf(name)) {
        m_settings.speechStyle.clear();
    }
    const int maxOff = qMax(0, result.catalog.styles.size() + 1 - kTilePage);
    if (m_stylePage > maxOff) {
        m_stylePage = maxOff;
    }
    storeStyleCatalog(result.catalog);
}

void ComposeUi::beginStyleName()
{
    if (m_borrowed == BorrowedPhrase::Tag) {
        endBorrowedPhrase();
    }
    if (m_borrowed != BorrowedPhrase::Style) {
        if (nameEditing()) {
            finishItemEditor(true);
        } else {
            endIconPick();
        }
        if (m_interaction != Interaction::Use) {
            m_interaction = Interaction::Use;
        }
        m_nameEditBackup = m_buffer.text();
        m_borrowed = BorrowedPhrase::Style;
        m_editId.clear();
        m_editColor.clear();
        m_editIcon.clear();
        m_buffer.load(QString());
    }
    m_styleEditor = true;
    m_styleSearch = true;
    rebuildBoard();
    if (!hasLive(kStylesLiveId)) {
        QString err;
        (void)presentLive(QString(kStylesLiveId), buildStylesDocument(), &err);
    } else {
        rebuildStyles();
    }
    if (isOpen()) {
        showComposeLayer(1);
    }
}

bool ComposeUi::saveStyleName(QString* error)
{
    if (m_borrowed != BorrowedPhrase::Style) {
        return true;
    }
    const QString name = m_buffer.text().trimmed();
    if (VoiceStyles::normalizeStyleName(name).isEmpty()) {
        const QString msg = QStringLiteral("Type a style name.");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const VoiceStyles::Result added = VoiceStyles::addStyle(styleCatalog(), name);
    if (!added.ok) {
        const QString msg = added.reason == QLatin1String("duplicate")
                                ? QStringLiteral("Already in the list.")
                                : QStringLiteral("Type a style name.");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    m_styleEditor = true;
    m_stylePage = qMax(0, added.catalog.styles.size() - 1);
    m_borrowed = BorrowedPhrase::None;
    m_buffer.load(m_nameEditBackup);
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();
    m_styleSearch = true;
    m_styleQuery.clear();
    rebuildBoard();
    storeStyleCatalog(added.catalog);
    notify(QStringLiteral("Added"));
    return true;
}

void ComposeUi::cancelStyleName()
{
    if (m_borrowed != BorrowedPhrase::Style) {
        return;
    }
    cancelBorrowedPhrase();
}

bool ComposeUi::openStyles(QString* error)
{
    m_styleEditor = false;
    m_styleSearch = true;
    m_styleQuery.clear();
    m_stylePage = 0;
    const bool ok = presentLive(QString(kStylesLiveId), buildStylesDocument(), error);
    if (ok && isOpen()) {
        showComposeLayer(1);
    }
    return ok;
}

void ComposeUi::rebuildStyles()
{
    if (!hasLive(kStylesLiveId)) {
        return;
    }
    QString err;
    PageDocument doc = buildStylesDocument();
    doc.id = QString(kStylesLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

PageDocument ComposeUi::buildStylesDocument()
{
    PageDocument doc;
    doc.id = QString(kStylesLiveId);
    doc.name = QStringLiteral("Styles");
    const ThemeColors theme = m_settings.resolvedTheme();
    const bool naming = m_borrowed == BorrowedPhrase::Style;
    const int sheetRows = naming ? 4 : 3;
    initTopOverlay(doc, 1, sheetRows, QVector<double>(sheetRows, 1.0), 8, 16, theme,
                   aboveKeyboardHeight(m_composeAuthored), speakBoardWidth());
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = {trackPx(72), trackPx(72), PageTrackSize::starWeight()};
    if (naming) {
        grid.rowTracks.push_back(trackPx(72));
    }
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const VoiceStyles::Catalog catalog = styleCatalog();
    const QString active = VoiceStyles::normalizeStyleName(m_settings.speechStyle);
    const QString activeKey = VoiceStyles::keyOf(active);

    grid.subGrids.push_back(compose_detail::speakTitleBar(
        m_styleEditor ? QStringLiteral("Edit styles") : QStringLiteral("Style"), naming, key));

    if (naming) {
        PageCell field = searchField(QStringLiteral("style_search"), m_buffer.text(),
                                     QStringLiteral("Style name"), 1, 0, 1, key, accent);
        field.caretIndex = m_buffer.text().isEmpty()
                               ? 0
                               : qBound(0, m_buffer.caret(), field.label.size());
        grid.cells.push_back(std::move(field));
    } else {
        grid.cells.push_back(searchField(QStringLiteral("style_search"), m_styleQuery,
                                         QStringLiteral("Search styles"), 1, 0, 1, key, accent));
    }

    QStringList names = m_styleEditor || catalog.favorites.isEmpty() ? catalog.styles
                                                                    : catalog.favorites;
    if (!m_styleEditor && !active.isEmpty()) {
        bool found = false;
        for (const QString& name : names) {
            if (VoiceStyles::keyOf(name) == activeKey) {
                found = true;
                break;
            }
        }
        if (!found) {
            names.prepend(active);
        }
    }
    if (!naming && !m_styleQuery.trimmed().isEmpty()) {
        QStringList matched;
        const QString query = m_styleQuery.trimmed();
        for (const QString& name : names) {
            if (name.contains(query, Qt::CaseInsensitive)) {
                matched.push_back(name);
            }
        }
        names = matched;
    }
    const bool showNone = !m_styleEditor;
    const int lead = showNone ? 1 : 0;
    const int addSlot = m_styleEditor && !naming ? 1 : 0;
    const int total = lead + names.size() + addSlot;
    const int maxOff = qMax(0, total - kTilePage);
    m_stylePage = qBound(0, m_stylePage, maxOff);
    const int start = m_stylePage;

    PageGrid body = overlayBody(2, 1);
    PageGrid list = overlayList(kTileCols, kTileRows);
    list.gapPx = 8;
    for (int slot = 0; slot < kTilePage; ++slot) {
        const int index = start + slot;
        const int tileRow = slot / kTileCols;
        const int tileCol = slot % kTileCols;
        if (index >= total) {
            list.cells.push_back(cell(QStringLiteral("style_name_%1").arg(slot), {}, tileRow,
                                      tileCol, {}, value, 1, QStringLiteral("label")));
            continue;
        }
        if (showNone && index == 0) {
            PageCell none = cell(QStringLiteral("style_none"), QStringLiteral("None"), tileRow,
                                 tileCol, QStringLiteral("compose.clearStyle"), key);
            none.style.radius = PageBox::all(12);
            if (active.isEmpty()) {
                paintSelected(none, accent, key);
            }
            list.cells.push_back(std::move(none));
            continue;
        }
        if (addSlot && index == lead + names.size()) {
            PageCell add = cell(QStringLiteral("style_add"), QStringLiteral("Add"), tileRow, tileCol,
                                QStringLiteral("compose.beginStyleName"), key, 1, {}, {},
                                QStringLiteral("add"));
            add.style.radius = PageBox::all(12);
            paintSurface(add, key);
            list.cells.push_back(std::move(add));
            continue;
        }
        const QString& name = names.at(index - lead);
        const QString encoded = encodeStyleName(name);
        const bool on = !activeKey.isEmpty() && VoiceStyles::keyOf(name) == activeKey;
        if (!m_styleEditor) {
            PageCell tile = cell(QStringLiteral("style_name_%1").arg(slot), name, tileRow, tileCol,
                                 QStringLiteral("compose.applyStyle.%1").arg(encoded), key);
            tile.activeState =
                QStringLiteral("compose.style.%1").arg(encodeStyleName(VoiceStyles::keyOf(name)));
            tile.style.radius = PageBox::all(12);
            if (on) {
                paintSelected(tile, accent, key);
            }
            list.cells.push_back(std::move(tile));
            continue;
        }
        PageGrid tile = makeNested(QStringLiteral("style_tile_%1").arg(slot), tileRow, tileCol, 1, 2,
                                   4);
        tile.columnTracks = {PageTrackSize::starWeight(), trackPx(56)};
        const bool fav = VoiceStyles::isFavorite(catalog, name);
        PageCell label = cell(QStringLiteral("style_name_%1").arg(slot), name, 0, 0,
                              QStringLiteral("compose.favStyle.%1").arg(encoded), fav ? accent : key,
                              1, {}, {}, fav ? QStringLiteral("star") : QString());
        label.style.radius = PageBox::all(12);
        tile.cells.push_back(std::move(label));
        PageCell trash = cell(QStringLiteral("style_trash_%1").arg(slot), {}, 0, 1,
                              QStringLiteral("compose.trashStyle.%1").arg(encoded), QColor(), 1, {},
                              {}, QStringLiteral("delete"));
        paintDanger(trash, theme.danger.isValid() ? theme.danger : QColor(180, 80, 80));
        tile.cells.push_back(std::move(trash));
        list.subGrids.push_back(std::move(tile));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kTilePage, total, key,
                                            QStringLiteral("compose.stylePage.-1"),
                                            QStringLiteral("compose.stylePage.1")));
    grid.subGrids.push_back(std::move(body));

    if (naming) {
        PageGrid foot = makeNested(QStringLiteral("footer"), 3, 0, 1, 2, 8);
        foot.columnTracks = {PageTrackSize::starWeight(1.4), PageTrackSize::starWeight()};
        PageCell save = cell(QStringLiteral("style_save"), QStringLiteral("Save"), 0, 0,
                             QStringLiteral("compose.saveName"), accent);
        paintSave(save, accent);
        foot.cells.push_back(std::move(save));
        PageCell cancel = cell(QStringLiteral("style_cancel"), QStringLiteral("Cancel"), 0, 1,
                               QStringLiteral("compose.cancelName"), key);
        paintSurface(cancel, key);
        foot.cells.push_back(std::move(cancel));
        grid.subGrids.push_back(std::move(foot));
    }
    return doc;
}

} // namespace gazer
