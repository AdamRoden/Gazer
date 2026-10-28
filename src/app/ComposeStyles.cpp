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
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using compose_detail::commandAction;

namespace {

QString encodeStyleName(const QString& name)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(name.toUtf8()));
}

PageAction showLayerAction(int layer)
{
    PageAction action;
    action.type = PageActionType::ShowLayers;
    action.layers = {layer};
    return action;
}

constexpr int kStylePickerCap = 12;
constexpr int kStyleEditorCols = 3;
constexpr int kStyleGroupCols = 4;

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
    refresh();
}

void ComposeUi::closeStyleEditor()
{
    m_styleEditor = false;
    m_stylePage = 0;
    refresh();
}

void ComposeUi::pageStyles(int delta)
{
    if (!m_styleEditor || delta == 0) {
        return;
    }
    const int pages = qMax(1, (styleCatalog().styles.size() + kStyleEditorPage - 1) / kStyleEditorPage);
    m_stylePage = qBound(0, m_stylePage + delta, pages - 1);
    refresh();
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
    const int pages = qMax(1, (result.catalog.styles.size() + kStyleEditorPage - 1) / kStyleEditorPage);
    if (m_stylePage >= pages) {
        m_stylePage = pages - 1;
    }
    storeStyleCatalog(result.catalog);
}

void ComposeUi::beginStyleName()
{
    if (!m_styleNaming) {
        if (nameEditing()) {
            finishItemEditor(true);
        } else if (hasLive(kItemEditLiveId)) {
            closeItemEdit();
        }
        if (m_interaction != Interaction::Use) {
            m_interaction = Interaction::Use;
        }
        m_nameEditBackup = m_buffer.text();
        m_styleNaming = true;
        m_editId.clear();
        m_editIcons = false;
        m_editColor.clear();
        m_editIcon.clear();
        m_buffer.load(QString());
    }
    m_styleEditor = true;
    rebuildBoard();
    showComposeLayer(1);
}

bool ComposeUi::saveStyleName(QString* error)
{
    if (!m_styleNaming) {
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
    m_stylePage = qMax(0, (added.catalog.styles.size() - 1) / kStyleEditorPage);
    m_styleNaming = false;
    m_buffer.load(m_nameEditBackup);
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();
    rebuildBoard();
    storeStyleCatalog(added.catalog);
    showComposeLayer(6);
    notify(QStringLiteral("Added"));
    return true;
}

void ComposeUi::cancelStyleName()
{
    if (!m_styleNaming) {
        return;
    }
    m_styleNaming = false;
    m_styleEditor = true;
    m_buffer.load(m_nameEditBackup);
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();
    rebuildBoard();
    showComposeLayer(6);
}

void ComposeUi::stampStyleLayer(PageDocument& doc) const
{
    PageGrid* grid = doc.findGrid(QStringLiteral("keys_style"));
    if (!grid) {
        return;
    }
    if (m_styleEditor) {
        stampStyleEditor(*grid);
    } else {
        stampStylePicker(*grid);
    }
}

void ComposeUi::stampStylePicker(PageGrid& grid) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const VoiceStyles::Catalog catalog = styleCatalog();
    QStringList shown = catalog.favorites.isEmpty() ? catalog.styles : catalog.favorites;
    if (shown.size() > kStylePickerCap) {
        shown = shown.mid(0, kStylePickerCap);
    }
    const QString active = VoiceStyles::normalizeStyleName(m_settings.speechStyle);
    if (!active.isEmpty()) {
        const QString activeKey = VoiceStyles::keyOf(active);
        bool found = false;
        for (const QString& name : shown) {
            if (VoiceStyles::keyOf(name) == activeKey) {
                found = true;
                break;
            }
        }
        if (!found) {
            shown.prepend(active);
            if (shown.size() > kStylePickerCap) {
                shown.removeLast();
            }
        }
    }

    grid.rows = 3;
    grid.columns = 5;
    grid.cells.clear();
    for (int i = 0; i < shown.size() && i < kStylePickerCap; ++i) {
        PageCell styleCell;
        styleCell.id = QStringLiteral("style_%1").arg(i);
        styleCell.row = i / 5;
        styleCell.col = i % 5;
        styleCell.label = shown.at(i);
        const QString key = VoiceStyles::keyOf(shown.at(i));
        const bool on = !active.isEmpty() && VoiceStyles::keyOf(active) == key;
        if (on) {
            styleCell.style.background = accent;
            styleCell.style.foreground = ThemeColors::contrastOn(accent);
        }
        styleCell.activeState = QStringLiteral("compose.style.%1").arg(encodeStyleName(key));
        styleCell.actions.push_back(
            commandAction(QStringLiteral("compose.applyStyle.%1").arg(encodeStyleName(shown.at(i)))));
        grid.cells.push_back(std::move(styleCell));
    }
    if (shown.isEmpty()) {
        PageCell empty;
        empty.id = QStringLiteral("style_empty");
        empty.row = 0;
        empty.col = 0;
        empty.colSpan = 5;
        empty.role = QStringLiteral("label");
        empty.label = QStringLiteral("No styles");
        grid.cells.push_back(std::move(empty));
    }
    PageCell edit;
    edit.id = QStringLiteral("style_edit");
    edit.row = 2;
    edit.col = 2;
    edit.icon = QStringLiteral("edit");
    edit.label = QStringLiteral("Edit");
    edit.actions.push_back(commandAction(QStringLiteral("compose.openStyleEditor")));
    grid.cells.push_back(std::move(edit));
    PageCell none;
    none.id = QStringLiteral("style_none");
    none.row = 2;
    none.col = 3;
    none.label = QStringLiteral("None");
    none.actions.push_back(commandAction(QStringLiteral("compose.clearStyle")));
    grid.cells.push_back(std::move(none));
    PageCell back;
    back.id = QStringLiteral("style_back");
    back.row = 2;
    back.col = 4;
    back.label = QStringLiteral("Back");
    back.actions.push_back(showLayerAction(5));
    grid.cells.push_back(std::move(back));
}

void ComposeUi::stampStyleEditor(PageGrid& grid) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor surface = theme.defaultCell();
    const VoiceStyles::Catalog catalog = styleCatalog();
    const QString active = VoiceStyles::normalizeStyleName(m_settings.speechStyle);
    const QString activeKey = VoiceStyles::keyOf(active);
    const int count = catalog.styles.size();
    const int pages = qMax(1, (count + kStyleEditorPage - 1) / kStyleEditorPage);
    const int page = qBound(0, m_stylePage, pages - 1);

    grid.rows = 3;
    grid.columns = kStyleEditorCols * kStyleGroupCols;
    grid.cells.clear();
    if (count == 0) {
        PageCell empty;
        empty.id = QStringLiteral("style_empty");
        empty.row = 0;
        empty.col = 0;
        empty.rowSpan = 2;
        empty.colSpan = grid.columns;
        empty.role = QStringLiteral("label");
        empty.label = QStringLiteral("No styles");
        grid.cells.push_back(std::move(empty));
    }
    for (int slot = 0; slot < kStyleEditorPage; ++slot) {
        const int index = page * kStyleEditorPage + slot;
        if (index >= count) {
            break;
        }
        const int row = slot / kStyleEditorCols;
        const int col = (slot % kStyleEditorCols) * kStyleGroupCols;
        const QString& name = catalog.styles.at(index);
        const QString encoded = encodeStyleName(name);
        const bool on = !activeKey.isEmpty() && VoiceStyles::keyOf(name) == activeKey;
        const bool fav = VoiceStyles::isFavorite(catalog, name);
        PageCell label;
        label.id = QStringLiteral("style_name_%1").arg(slot);
        label.row = row;
        label.col = col;
        label.colSpan = 2;
        label.label = name;
        if (on) {
            label.style.background = accent;
            label.style.foreground = ThemeColors::contrastOn(accent);
        }
        label.activeState = QStringLiteral("compose.style.%1").arg(encodeStyleName(VoiceStyles::keyOf(name)));
        label.actions.push_back(commandAction(QStringLiteral("compose.applyStyle.%1").arg(encoded)));
        grid.cells.push_back(std::move(label));

        PageCell star = cell(QStringLiteral("style_star_%1").arg(slot),
                             fav ? QStringLiteral("Starred") : QStringLiteral("Star"), row, col + 2,
                             QStringLiteral("compose.favStyle.%1").arg(encoded),
                             fav ? accent : surface, 1, {}, {}, QStringLiteral("star"));
        grid.cells.push_back(std::move(star));

        PageCell trash = cell(QStringLiteral("style_trash_%1").arg(slot), QStringLiteral("Trash"),
                              row, col + 3, QStringLiteral("compose.trashStyle.%1").arg(encoded),
                              surface, 1, {}, {}, QStringLiteral("delete"));
        grid.cells.push_back(std::move(trash));
    }

    PageCell add = cell(QStringLiteral("style_add"), QStringLiteral("Add"), 2, 0,
                        QStringLiteral("compose.beginStyleName"), surface, 3);
    grid.cells.push_back(std::move(add));

    const bool canPrev = page > 0;
    PageCell prev = cell(QStringLiteral("style_prev"), QStringLiteral("Prev"), 2, 3,
                         canPrev ? QStringLiteral("compose.stylePage.-1") : QString(), surface, 2,
                         canPrev ? QString() : QStringLiteral("label"));
    grid.cells.push_back(std::move(prev));

    const bool canNext = page + 1 < pages;
    PageCell next = cell(QStringLiteral("style_next"), QStringLiteral("Next"), 2, 5,
                         canNext ? QStringLiteral("compose.stylePage.1") : QString(), surface, 2,
                         canNext ? QString() : QStringLiteral("label"));
    if (pages > 1) {
        next.caption = QStringLiteral("%1/%2").arg(page + 1).arg(pages);
    }
    grid.cells.push_back(std::move(next));

    PageCell none = cell(QStringLiteral("style_none"), QStringLiteral("None"), 2, 7,
                         QStringLiteral("compose.clearStyle"), surface, 2);
    grid.cells.push_back(std::move(none));
    PageCell done = cell(QStringLiteral("style_done"), QStringLiteral("Done"), 2, 9,
                         QStringLiteral("compose.closeStyleEditor"), surface, 3);
    grid.cells.push_back(std::move(done));
}

} // namespace gazer
