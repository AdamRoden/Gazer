#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/IconCatalog.h"
#include "app/SettingsPageBuild.h"
#include "assist/SoundboardStore.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "predict/TypingContext.h"
#include "ui/KeySymbols.h"
#include "ui/Theme.h"

#include <QVector>

namespace gazer {

using SettingsPageBuild::adoptCallerBoard;
using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsPageBuild::makeNested;
using compose_detail::parseColor;

namespace {

constexpr int kIconTileCols = 8;
constexpr int kIconTileRows = 5;

QString storedEditColor(const QColor& c)
{
    return c.alpha() == 255 ? c.name(QColor::HexRgb) : c.name(QColor::HexArgb);
}

/// Scroll units are rows. The first visible icon is always column 0 of a row.
int iconMaxRow(int count)
{
    const int rows = (qMax(0, count) + kIconTileCols - 1) / kIconTileCols;
    return qMax(0, rows - kIconTileRows);
}

PageGrid iconScrollbar(int row, int count, const QColor& surface)
{
    const int totalRows = qMax(1, (qMax(0, count) + kIconTileCols - 1) / kIconTileCols);
    const int maxRow = iconMaxRow(count);
    PageGrid scroll = makeNested(QStringLiteral("icon_scroll"), 0, 1, 3, 1, 6);
    scroll.rowTracks = {PageTrackSize::fromDim(PageDim::pixels(72)), PageTrackSize::starWeight(),
                        PageTrackSize::fromDim(PageDim::pixels(72))};
    scroll.style.background = surface;
    scroll.style.thickness = PageBox::all(0);
    scroll.style.radius = PageBox::all(16);
    auto endButton = [&](const QString& id, int buttonRow, bool on, const QString& command,
                         const QString& icon) {
        PageCell button = cell(id, {}, buttonRow, 0, on ? command : QString(), surface, 1,
                               on ? QString() : QStringLiteral("label"), {}, icon);
        if (!on) {
            button.style.foreground =
                ThemeColors::mix(surface, ThemeColors::contrastOn(surface), 0.4);
        }
        return button;
    };
    scroll.cells.push_back(endButton(QStringLiteral("sb_up"), 0, row > 0,
                                     QStringLiteral("compose.iconRow.-1"),
                                     QStringLiteral("ArrowUp")));
    PageCell track = cell(QStringLiteral("sb_track"), {}, 1, 0, {}, QColor(), 1,
                          QStringLiteral("scrollbar"));
    track.caption = ScrollBar::formatSpec(row, kIconTileRows, totalRows);
    track.dwell.scanGrace = kTrackLockMs;
    track.style.thickness = PageBox::all(0);
    track.style.radius = PageBox::all(10);
    scroll.cells.push_back(std::move(track));
    scroll.cells.push_back(endButton(QStringLiteral("sb_down"), 2, row < maxRow,
                                     QStringLiteral("compose.iconRow.1"),
                                     QStringLiteral("ArrowDown")));
    return scroll;
}

int iconDraftIndex(const QString& draft, const QStringList& hits)
{
    if (draft.isEmpty()) {
        return 0;
    }
    for (int i = 0; i < hits.size(); ++i) {
        if (hits.at(i).compare(draft, Qt::CaseInsensitive) == 0) {
            return i + 1;
        }
    }
    return -1;
}

} // namespace

void ComposeUi::closeItemEdit()
{
    if (hasLive(kItemEditLiveId)) {
        m_pages.closePage(QString(kItemEditLiveId));
    }
}

bool ComposeUi::nameEditCanDelete() const
{
    switch (m_nameEdit) {
    case NameEditKind::Topic:
    case NameEditKind::Pin:
        return true;
    default:
        return false;
    }
}

void ComposeUi::endNameEdit(bool restorePhrase)
{
    if (restorePhrase) {
        m_buffer.load(m_nameEditBackup);
        if (m_typing) {
            m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
        }
    }
    m_nameEdit = NameEditKind::None;
    m_editId.clear();
    m_nameEditBackup.clear();
}

bool ComposeUi::beginNameEdit(NameEditKind kind, const QString& target, const QString& initial)
{
    if (kind == NameEditKind::None) {
        return false;
    }
    if (borrowingPhrase()) {
        endBorrowedPhrase();
    }
    if (nameEditing()) {
        endNameEdit(true);
    }
    if (m_dismissColor) {
        m_dismissColor();
    }
    endIconPick();
    m_nameEditBackup = m_buffer.text();
    m_nameEdit = kind;
    m_editId = target;
    m_interaction = Interaction::NameEdit;
    loadEditAppearance();
    if (m_formName.isEmpty()) {
        m_formName = initial;
    }
    m_buffer.load(m_formName);
    QString err;
    if (!presentLive(QString(kFormLiveId), buildFormDocument(), &err)) {
        finishItemEditor(true);
        return false;
    }
    if (isOpen()) {
        showComposeLayer(1);
    }
    rebuildBoard();
    return true;
}

void ComposeUi::finishItemEditor(bool restorePhrase)
{
    if (m_dismissColor) {
        m_dismissColor();
    }
    endNameEdit(restorePhrase);
    m_interaction = Interaction::Use;
    endIconPick();
    m_editColor.clear();
    m_editIcon.clear();
    m_formField = FormField::Name;
    m_formName.clear();
    m_formPhrase.clear();
    m_formPhraseTouched = false;
    if (hasLive(kFormLiveId)) {
        m_pages.closePage(QString(kFormLiveId));
    }
    rebuildBoard();
}

void ComposeUi::cancelNameEdit()
{
    if (borrowingPhrase()) {
        cancelBorrowedPhrase();
        return;
    }
    if (!nameEditing() && !hasLive(kItemEditLiveId)) {
        return;
    }
    finishItemEditor(true);
}

void ComposeUi::loadEditAppearance()
{
    m_editColor.clear();
    m_editIcon.clear();
    m_formField = FormField::Name;
    m_formPhraseTouched = false;
    m_formName.clear();
    m_formPhrase.clear();
    m_formIndex = 0;
    m_formCols = 4;
    if (const SoundboardTopic* t = m_board.findTopic(m_editId);
        m_nameEdit == NameEditKind::Topic && t) {
        m_editColor = t->color;
        m_editIcon = t->icon;
        m_formName = t->name;
        m_formCols = t->gridCols;
        m_formIndex = qMax(0, formLiveIndex());
        return;
    }
    if (const SoundboardButton* b = m_board.findButton(m_editId);
        m_nameEdit == NameEditKind::Pin && b) {
        m_editColor = b->color;
        m_editIcon = b->icon;
        m_formName = b->label;
        m_formPhrase = b->utteranceText;
        m_formIndex = qMax(0, formLiveIndex());
    }
}

void ComposeUi::persistEditAppearance()
{
    QString err;
    if (m_nameEdit == NameEditKind::Topic) {
        (void)m_board.setTopicColor(m_editId, m_editColor, &err);
        (void)m_board.setTopicIcon(m_editId, m_editIcon, &err);
        return;
    }
    if (m_nameEdit == NameEditKind::Pin) {
        (void)m_board.setButtonColor(m_editId, m_editColor, &err);
        (void)m_board.setButtonIcon(m_editId, m_editIcon, &err);
    }
}

bool ComposeUi::presentItemEdit(QString* error)
{
    if (!presentLive(QString(kItemEditLiveId), buildItemEditDocument(), error)) {
        return false;
    }
    prefetchVisibleIcons();
    return true;
}

void ComposeUi::rebuildItemEdit()
{
    if (!nameEditing() || !hasLive(kItemEditLiveId)) {
        return;
    }
    QString err;
    (void)presentItemEdit(&err);
}

PageDocument ComposeUi::buildItemEditDocument() const
{
    PageDocument doc;
    doc.id = QString(kItemEditLiveId);
    doc.name = QStringLiteral("Icon");
    const ThemeColors theme = m_settings.resolvedTheme();
    initGrid(doc, 1, 3, 1080, 1080, 6, 12, theme);
    PageGrid& grid = doc.grids[0];
    adoptCallerBoard(grid, m_pages.pageBehind(QString(kItemEditLiveId)));
    if (m_iconKeyboard) {
        grid.anchor = PageAnchor::Top;
        grid.offset.x = PageDim::pixels(0);
        grid.offset.y = PageDim::pixels(0);
        grid.size.y = PageDim::expression(QStringLiteral("A_ScreenHeight/12*7"));
    }
    grid.rowTracks = starTracks({1.35, 11.0, 2.2});
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QStringList hits = iconHits();
    const int total = hits.size();
    const int row = qBound(0, m_iconRow, iconMaxRow(total));
    const int start = row * kIconTileCols;
    const int selected = iconDraftIndex(m_iconDraft, hits);
    const bool cleared = m_iconDraft.isEmpty();

    const QString query = m_iconQuery;
    PageCell search = cell(QStringLiteral("icon_search"),
                           query.isEmpty() ? QStringLiteral("Search") : query, 0, 0,
                           QStringLiteral("compose.iconSearch"), m_iconKeyboard ? accent : key, 1,
                           {}, {}, QStringLiteral("search"));
    search.textStyle = QStringLiteral("title");
    if (m_iconKeyboard && !query.isEmpty()) {
        search.caretIndex = query.size();
    }
    if (m_iconKeyboard) {
        search.style.borderColor = accent;
        search.style.thickness = PageBox::all(3);
    }
    QString status = m_iconCatalog ? m_iconCatalog->statusLabel() : QString();
    if (!query.trimmed().isEmpty()) {
        status += QStringLiteral(" · ") + query.trimmed();
    }
    if (!hits.isEmpty()) {
        status += QStringLiteral(" · %1").arg(hits.size());
    }
    search.caption = status;
    grid.cells.push_back(std::move(search));

    PageGrid body = makeNested(QStringLiteral("icon_body"), 1, 0, 1, 2, 6);
    body.columnTracks = {PageTrackSize::starWeight(),
                         PageTrackSize::fromDim(PageDim::pixels(72))};
    if (hits.isEmpty()) {
        PageGrid empty = makeNested(QStringLiteral("icon_tiles"), 0, 0, 1, 1, 8);
        empty.cells.push_back(cell(QStringLiteral("icon_empty"),
                                   QStringLiteral("No matching icons"), 0, 0, {}, key, 1,
                                   QStringLiteral("label")));
        body.subGrids.push_back(std::move(empty));
    } else {
        PageGrid tiles =
            makeNested(QStringLiteral("icon_tiles"), 0, 0, kIconTileRows, kIconTileCols, 8);
        for (int row = 0; row < kIconTileRows; ++row) {
            tiles.rowTracks.push_back(PageTrackSize::starWeight());
        }
        for (int col = 0; col < kIconTileCols; ++col) {
            tiles.columnTracks.push_back(PageTrackSize::starWeight());
        }
        const int page = kIconTileCols * kIconTileRows;
        for (int slot = 0; slot < page; ++slot) {
            const int nameIndex = start + slot;
            if (nameIndex >= total) {
                break;
            }
            const int pick = nameIndex + 1;
            PageCell tile =
                cell(QStringLiteral("icon_%1").arg(slot), {}, slot / kIconTileCols,
                     slot % kIconTileCols, QStringLiteral("compose.iconPick.%1").arg(pick),
                     pick == selected ? accent : key, 1, {}, {}, hits.at(nameIndex));
            tile.style.radius = PageBox::all(12);
            tiles.cells.push_back(std::move(tile));
        }
        body.subGrids.push_back(std::move(tiles));
    }
    body.subGrids.push_back(iconScrollbar(row, total, key));
    grid.subGrids.push_back(std::move(body));

    PageGrid actions = makeNested(QStringLiteral("icon_actions"), 2, 0, 1, 4, 8);
    actions.columnTracks = starTracks({1.15, 1.7, 1.15, 1.15});
    PageCell clear = cell(QStringLiteral("icon_clear"), QStringLiteral("Clear"), 0, 0,
                          QStringLiteral("compose.iconPick.0"), cleared ? accent : key);
    actions.cells.push_back(std::move(clear));
    PageCell current = cell(QStringLiteral("icon_current"),
                            m_iconDraft.isEmpty() ? QStringLiteral("None") : QString(), 0, 1, {},
                            m_iconDraft.isEmpty() ? key : accent, 1, QStringLiteral("label"), {},
                            m_iconDraft);
    current.style.radius = PageBox::all(12);
    actions.cells.push_back(std::move(current));
    PageCell save = cell(QStringLiteral("icon_save"), QStringLiteral("Save"), 0, 2,
                         QStringLiteral("compose.iconSave"), accent);
    save.style.foreground = ThemeColors::contrastOn(accent);
    actions.cells.push_back(std::move(save));
    actions.cells.push_back(cell(QStringLiteral("icon_cancel"), QStringLiteral("Cancel"), 0, 3,
                                 QStringLiteral("compose.iconCancel"), key));
    grid.subGrids.push_back(std::move(actions));
    return doc;
}

bool ComposeUi::saveNameEdit(QString* error)
{
    if (borrowingPhrase()) {
        return saveBorrowedPhrase(error);
    }
    if (!nameEditing()) {
        return true;
    }
    QString name = formNameText();
    if (name.isEmpty()) {
        const QString msg = QStringLiteral("Type a name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    name = ellipsis(name, 24);

    bool ok = false;
    switch (m_nameEdit) {
    case NameEditKind::Pin:
        ok = m_board.setButtonLabel(m_editId, name, error);
        break;
    case NameEditKind::Topic:
        ok = m_board.renameTopic(m_editId, name, error);
        break;
    default:
        ok = true;
        break;
    }
    if (ok) {
        persistEditAppearance();
        applyFormDrafts();
    }
    if (!ok) {
        return false;
    }
    finishItemEditor(true);
    notify(QStringLiteral("Saved"));
    return true;
}

bool ComposeUi::deleteNameEdit(QString* error)
{
    if (borrowingPhrase()) {
        return true;
    }
    if (!nameEditCanDelete()) {
        finishItemEditor(true);
        return true;
    }
    bool ok = false;
    switch (m_nameEdit) {
    case NameEditKind::Pin:
        ok = m_board.removeButton(m_editId, error);
        break;
    case NameEditKind::Topic:
        ok = m_board.removeTopic(m_editId, error);
        if (!ok && error && !error->isEmpty()) {
            notify(*error);
        }
        break;
    default:
        ok = true;
        break;
    }
    if (!ok) {
        return false;
    }
    finishItemEditor(true);
    notify(QStringLiteral("Deleted"));
    return true;
}

bool ComposeUi::openPinEdit(const QString& buttonId, QString* error)
{
    const SoundboardButton* b = m_board.findButton(buttonId);
    if (!b) {
        if (error) {
            *error = QStringLiteral("Unknown button");
        }
        return false;
    }
    return beginNameEdit(NameEditKind::Pin, buttonId, b->label);
}

bool ComposeUi::openTopicEdit(const QString& topicId, QString* error)
{
    const SoundboardTopic* t = m_board.findTopic(topicId);
    if (!t) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    return beginNameEdit(NameEditKind::Topic, topicId, t->name);
}

void ComposeUi::showEditColors()
{
    if (!nameEditing()) {
        return;
    }
    if (m_editIcons || hasLive(kItemEditLiveId)) {
        endIconPick();
    }
    if (!m_colorPick) {
        notify(QStringLiteral("Color picker is not available"));
        rebuildBoard();
        return;
    }
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor fallback = theme.accent.isValid() ? theme.accent : QColor(30, 151, 243);
    const QColor initial = parseColor(m_editColor, fallback);
    QString err;
    const bool ok = m_colorPick(
        initial, QStringLiteral("Color"),
        [this](const QColor& chosen) {
            if (!nameEditing() || !chosen.isValid()) {
                return;
            }
            m_editColor = storedEditColor(chosen);
            rebuildBoard();
        },
        &err);
    if (!ok && !err.isEmpty()) {
        notify(err);
    }
    rebuildBoard();
}

void ComposeUi::showEditIcons()
{
    if (!nameEditing()) {
        return;
    }
    if (m_dismissColor) {
        m_dismissColor();
    }
    if (!m_editIcons) {
        m_iconDraft = m_editIcon;
        m_iconQuery.clear();
        m_iconKeyboard = false;
        m_editIcons = true;
        if (m_iconCatalog) {
            m_iconCatalog->ensureLoaded();
        }
        const QStringList hits = iconHits();
        const int index = iconDraftIndex(m_iconDraft, hits);
        const int nameIndex = index > 0 ? index - 1 : 0;
        const int startRow = index > 0 ? qMax(0, nameIndex / kIconTileCols - kIconTileRows / 2) : 0;
        m_iconRow = qBound(0, startRow, iconMaxRow(hits.size()));
    } else if (m_iconCatalog) {
        m_iconCatalog->ensureLoaded();
    }
    QString err;
    if (!presentItemEdit(&err)) {
        endIconPick();
        if (!err.isEmpty()) {
            notify(err);
        }
        rebuildBoard();
        return;
    }
    rebuildBoard();
}

bool ComposeUi::pickIcon(int index, QString* error)
{
    if (!nameEditing() || !m_editIcons) {
        if (error) {
            *error = QStringLiteral("Icon list is not open");
        }
        return false;
    }
    const QStringList hits = iconHits();
    if (index < 0 || index > hits.size()) {
        if (error) {
            *error = QStringLiteral("Unknown icon");
        }
        return false;
    }
    m_iconDraft = index == 0 ? QString() : hits.at(index - 1);
    rebuildItemEdit();
    return true;
}

void ComposeUi::saveIconPick()
{
    if (!nameEditing() || !m_editIcons) {
        return;
    }
    m_editIcon = m_iconDraft;
    endIconPick();
    rebuildBoard();
}

void ComposeUi::cancelIconPick()
{
    if (!m_editIcons && !hasLive(kItemEditLiveId)) {
        return;
    }
    endIconPick();
    rebuildBoard();
}

void ComposeUi::scrollIconRows(int rows)
{
    if (!m_editIcons || rows == 0) {
        return;
    }
    const int next = qBound(0, m_iconRow + rows, iconMaxRow(iconHits().size()));
    if (next == m_iconRow) {
        return;
    }
    m_iconRow = next;
    rebuildItemEdit();
}

void ComposeUi::iconsGoto(int row)
{
    if (!m_editIcons) {
        return;
    }
    const int next = qBound(0, row, iconMaxRow(iconHits().size()));
    if (next == m_iconRow) {
        return;
    }
    m_iconRow = next;
    rebuildItemEdit();
}

void ComposeUi::focusIconSearch()
{
    if (!nameEditing() || !m_editIcons || m_iconKeyboard) {
        return;
    }
    m_iconKeyboard = true;
    showComposeLayer(1);
    rebuildItemEdit();
    rebuildBoard();
}

void ComposeUi::closeIconKeyboard()
{
    if (!m_iconKeyboard) {
        return;
    }
    m_iconKeyboard = false;
    rebuildItemEdit();
    rebuildBoard();
}

bool ComposeUi::handleIconKeyboardEscape()
{
    if (!m_iconKeyboard) {
        return false;
    }
    closeIconKeyboard();
    return true;
}

bool ComposeUi::handleIconQuerySend(const PageAction& action)
{
    if (!m_iconKeyboard || !m_editIcons) {
        return false;
    }
    const QString add =
        compose_detail::queryInsertion(action, compose_detail::iconQueryChar);
    if (compose_detail::appendQuery(m_iconQuery, add)) {
        m_iconRow = 0;
        rebuildItemEdit();
    }
    return true;
}

bool ComposeUi::handleIconQueryCommand(const QString& command)
{
    if (!m_iconKeyboard || !m_editIcons) {
        return false;
    }
    using compose_detail::QueryCommand;
    switch (compose_detail::editQuery(m_iconQuery, command, compose_detail::iconQueryChar)) {
    case QueryCommand::Edited:
        m_iconRow = 0;
        rebuildItemEdit();
        break;
    case QueryCommand::Enter:
        closeIconKeyboard();
        break;
    case QueryCommand::Ignored:
        break;
    }
    return true;
}

void ComposeUi::clearIconSession()
{
    m_editIcons = false;
    m_iconDraft.clear();
    m_iconRow = 0;
    m_iconQuery.clear();
    m_iconKeyboard = false;
}

void ComposeUi::endIconPick()
{
    clearIconSession();
    if (m_listScroll == ListScroll::Icons) {
        stopListScroll();
    }
    closeItemEdit();
}

void ComposeUi::prefetchVisibleIcons()
{
    const QStringList hits = iconHits();
    const int row = qBound(0, m_iconRow, iconMaxRow(hits.size()));
    QStringList names = hits.mid(row * kIconTileCols, kIconTileCols * kIconTileRows);
    if (!m_iconDraft.isEmpty()) {
        names.push_back(m_iconDraft);
    }
    names.push_back(QStringLiteral("search"));
    prefetchIcons(names);
}

void ComposeUi::prefetchBoardIcons()
{
    QStringList names;
    for (const SoundboardTopic& topic : m_board.topics()) {
        if (!topic.icon.isEmpty()) {
            names.push_back(topic.icon);
        }
        for (const SoundboardButton& button : topic.buttons) {
            if (!button.icon.isEmpty()) {
                names.push_back(button.icon);
            }
        }
    }
    if (!m_iconDraft.isEmpty()) {
        names.push_back(m_iconDraft);
    }
    if (!m_editIcon.isEmpty()) {
        names.push_back(m_editIcon);
    }
    prefetchIcons(names);
}

void ComposeUi::onIconCatalogChanged()
{
    prefetchBoardIcons();
    if (m_editIcons) {
        rebuildItemEdit();
    }
}

void ComposeUi::onIconSvgsReady()
{
    if (m_editIcons || isOpen()) {
        m_pages.refreshDecorated();
    }
}

QStringList ComposeUi::iconHits() const
{
    return m_iconCatalog ? m_iconCatalog->search(m_iconQuery) : QStringList{};
}

void ComposeUi::prefetchIcons(const QStringList& names)
{
    if (!m_iconCatalog || names.isEmpty()) {
        return;
    }
    for (const QString& name : names) {
        if (!name.isEmpty() && !KeySymbols::contains(name)) {
            m_iconCatalog->ensureLoaded();
            break;
        }
    }
    m_iconCatalog->requestIcons(names);
}

} // namespace gazer
