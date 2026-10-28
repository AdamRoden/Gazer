#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "ui/ColorSwatches.h"
#include "assist/SoundboardStore.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QVector>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initTopOverlay;
using gazer::paletteColors;
using compose_detail::paletteIcons;
using compose_detail::parseColor;

namespace {

constexpr int kEditCols = 8;
constexpr int kColorRows = 5;

void initEditOverlay(PageDocument& doc, int rows, const QVector<double>& weights,
                     const ThemeColors& theme, const PageDim& width)
{
    initTopOverlay(doc, kEditCols, rows, weights, 8, 16, theme,
                   QStringLiteral("A_ScreenHeight/12*5+8"), width);
}

void addColorSwatches(PageGrid& grid, int startSlot, const QString& idPrefix,
                      const QString& cmdPrefix, const QString& current, const QColor& fallback,
                      const QColor& accent)
{
    const auto& colors = paletteColors();
    for (int i = 0; i < colors.size(); ++i) {
        const int slot = startSlot + i;
        const int row = slot / kEditCols;
        const int col = slot % kEditCols;
        const QColor bg = parseColor(colors.at(i), fallback);
        PageCell c = cell(QStringLiteral("%1%2").arg(idPrefix).arg(i), {}, row, col,
                          QStringLiteral("%1%2").arg(cmdPrefix).arg(i), bg);
        if (!current.isEmpty() && current.compare(colors.at(i), Qt::CaseInsensitive) == 0) {
            c.style.borderColor = accent;
            c.style.thickness = PageBox::all(4);
        }
        grid.cells.push_back(c);
    }
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
    if (m_styleNaming) {
        m_styleNaming = false;
        m_buffer.load(m_nameEditBackup);
        if (m_typing) {
            m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
        }
        m_nameEditBackup.clear();
    }
    if (nameEditing()) {
        endNameEdit(true);
    }
    m_nameEditBackup = m_buffer.text();
    m_nameEdit = kind;
    m_editId = target;
    m_editIcons = false;
    m_interaction = Interaction::NameEdit;
    loadEditAppearance();
    m_buffer.load(initial);
    QString err;
    if (!presentItemEdit(&err)) {
        endNameEdit(true);
        m_interaction = Interaction::Use;
        rebuildBoard();
        return false;
    }
    rebuildBoard();
    return true;
}

void ComposeUi::finishItemEditor(bool restorePhrase)
{
    endNameEdit(restorePhrase);
    m_interaction = Interaction::Use;
    m_editIcons = false;
    m_editColor.clear();
    m_editIcon.clear();
    closeItemEdit();
    rebuildBoard();
}

void ComposeUi::cancelNameEdit()
{
    if (m_styleNaming) {
        cancelStyleName();
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
    if (const SoundboardTopic* t = m_board.findTopic(m_editId);
        m_nameEdit == NameEditKind::Topic && t) {
        m_editColor = t->color;
        m_editIcon = t->icon;
        return;
    }
    if (const SoundboardButton* b = m_board.findButton(m_editId);
        m_nameEdit == NameEditKind::Pin && b) {
        m_editColor = b->color;
        m_editIcon = b->icon;
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
    return presentLive(QString(kItemEditLiveId), buildItemEditDocument(), error);
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
    doc.name = QStringLiteral("Edit");
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor key = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const PageDim width = speakBoardWidth();
    if (m_editIcons) {
        initEditOverlay(doc, 4, {1.0, 1.0, 1.0, 1.0}, theme, width);
        PageGrid& grid = doc.grids[0];
        PageCell none = cell(QStringLiteral("ti_none"), QStringLiteral("None"), 0, 0,
                             QStringLiteral("compose.editClearIcon"), key);
        if (m_editIcon.isEmpty()) {
            none.style.borderColor = accent;
            none.style.thickness = PageBox::all(4);
        }
        grid.cells.push_back(none);
        const auto& icons = paletteIcons();
        for (int i = 0; i < icons.size(); ++i) {
            const int slot = i + 1;
            const int row = slot / kEditCols;
            const int col = slot % kEditCols;
            PageCell c = cell(QStringLiteral("ti_%1").arg(i), {}, row, col,
                              QStringLiteral("compose.editIcon.%1").arg(i), key, 1, {}, {},
                              icons.at(i));
            if (!m_editIcon.isEmpty()
                && m_editIcon.compare(icons.at(i), Qt::CaseInsensitive) == 0) {
                c.style.background = accent;
                c.style.foreground = ThemeColors::contrastOn(accent);
            }
            grid.cells.push_back(c);
        }
    } else {
        initEditOverlay(doc, kColorRows, {1.0, 1.0, 1.0, 1.0, 1.0}, theme, width);
        PageGrid& grid = doc.grids[0];
        PageCell none = cell(QStringLiteral("c_none"), QStringLiteral("None"), 0, 0,
                             QStringLiteral("compose.editClearColor"), key);
        if (m_editColor.isEmpty()) {
            none.style.borderColor = accent;
            none.style.thickness = PageBox::all(4);
        }
        grid.cells.push_back(none);
        addColorSwatches(grid, 1, QStringLiteral("c_"), QStringLiteral("compose.editColor."),
                         m_editColor, key, accent);
    }
    return doc;
}

bool ComposeUi::saveNameEdit(QString* error)
{
    if (m_styleNaming) {
        return saveStyleName(error);
    }
    if (!nameEditing()) {
        return true;
    }
    const QString name = ellipsis(m_buffer.text().trimmed(), 24);
    if (name.isEmpty()) {
        const QString msg = QStringLiteral("Type a name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }

    bool ok = false;
    switch (m_nameEdit) {
    case NameEditKind::Pin:
        ok = m_board.setButtonLabel(m_editId, name, error);
        if (ok) {
            persistEditAppearance();
        }
        break;
    case NameEditKind::Topic:
        ok = m_board.renameTopic(m_editId, name, error);
        if (ok) {
            persistEditAppearance();
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
    notify(QStringLiteral("Saved"));
    return true;
}

bool ComposeUi::deleteNameEdit(QString* error)
{
    if (m_styleNaming) {
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
    m_editIcons = false;
    rebuildBoard();
    rebuildItemEdit();
}

void ComposeUi::showEditIcons()
{
    if (!nameEditing()) {
        return;
    }
    m_editIcons = true;
    rebuildBoard();
    rebuildItemEdit();
}

bool ComposeUi::setEditedColor(int index, QString* error)
{
    if (!nameEditing()) {
        if (error) {
            *error = QStringLiteral("Nothing selected");
        }
        return false;
    }
    const auto& colors = paletteColors();
    if (index < 0 || index >= colors.size()) {
        if (error) {
            *error = QStringLiteral("Unknown color");
        }
        return false;
    }
    m_editColor = colors.at(index);
    rebuildBoard();
    rebuildItemEdit();
    return true;
}

void ComposeUi::clearEditedColor()
{
    if (!nameEditing()) {
        return;
    }
    m_editColor.clear();
    rebuildBoard();
    rebuildItemEdit();
}

void ComposeUi::clearEditedIcon()
{
    if (!nameEditing()) {
        return;
    }
    m_editIcon.clear();
    rebuildBoard();
    rebuildItemEdit();
}

bool ComposeUi::setEditedIcon(int index, QString* error)
{
    if (!nameEditing()) {
        if (error) {
            *error = QStringLiteral("Nothing selected");
        }
        return false;
    }
    const auto& icons = paletteIcons();
    if (index < 0 || index >= icons.size()) {
        if (error) {
            *error = QStringLiteral("Unknown icon");
        }
        return false;
    }
    m_editIcon = icons.at(index);
    rebuildBoard();
    rebuildItemEdit();
    return true;
}

} // namespace gazer
