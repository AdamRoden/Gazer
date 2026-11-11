#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QUrl>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::cell;
using compose_detail::commandAction;

namespace {

constexpr int kTagCols = 6;
constexpr int kTagBodyRows = 2;
constexpr int kTagPage = kTagCols * kTagBodyRows;

QString encodeTag(const QString& name)
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

} // namespace

bool ComposeUi::borrowingPhrase() const
{
    return m_borrowed != BorrowedPhrase::None;
}

void ComposeUi::endBorrowedPhrase()
{
    if (m_borrowed == BorrowedPhrase::None) {
        return;
    }
    const BorrowedPhrase kind = m_borrowed;
    const int caret = m_borrowCaret;
    m_borrowed = BorrowedPhrase::None;
    m_buffer.load(m_nameEditBackup);
    if (kind == BorrowedPhrase::Tag) {
        m_buffer.setCaret(caret);
    }
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();
}

void ComposeUi::cancelBorrowedPhrase()
{
    if (m_borrowed == BorrowedPhrase::None) {
        return;
    }
    const int layer = m_borrowed == BorrowedPhrase::Style ? 6 : 7;
    if (m_borrowed == BorrowedPhrase::Style) {
        m_styleEditor = true;
    }
    endBorrowedPhrase();
    rebuildBoard();
    showComposeLayer(layer);
}

bool ComposeUi::saveBorrowedPhrase(QString* error)
{
    if (m_borrowed == BorrowedPhrase::Style) {
        return saveStyleName(error);
    }
    if (m_borrowed == BorrowedPhrase::Tag) {
        return saveTagName(error);
    }
    return true;
}

void ComposeUi::insertSavedTag(const QString& encoded)
{
    if (borrowingPhrase() || nameEditing()) {
        return;
    }
    const QString name =
        AppSettings::normalizeSpeechTag(QUrl::fromPercentEncoding(encoded.toUtf8()));
    if (name.isEmpty()) {
        return;
    }
    m_buffer.insertTag(name);
    commitEdit();
    showComposeLayer(5);
}

void ComposeUi::pageTags(int delta)
{
    const int pages = qMax(1, (m_settings.savedSpeechTags.size() + kTagPage - 1) / kTagPage);
    m_tagPage = qBound(0, m_tagPage + delta, pages - 1);
    refresh();
}

void ComposeUi::beginTagName()
{
    if (m_borrowed == BorrowedPhrase::Style) {
        cancelBorrowedPhrase();
    }
    if (nameEditing()) {
        finishItemEditor(true);
    } else {
        endIconPick();
    }
    if (m_interaction != Interaction::Use) {
        m_interaction = Interaction::Use;
    }
    if (m_borrowed != BorrowedPhrase::Tag) {
        m_borrowCaret = m_buffer.caret();
        m_nameEditBackup = m_buffer.text();
        m_borrowed = BorrowedPhrase::Tag;
        m_buffer.load(QString());
    }
    rebuildBoard();
    showComposeLayer(1);
}

bool ComposeUi::saveTagName(QString* error)
{
    if (m_borrowed != BorrowedPhrase::Tag) {
        return true;
    }
    QString name = AppSettings::normalizeSpeechTag(m_buffer.text());
    if (name.size() > VoiceStyles::kNameMax) {
        name = name.left(VoiceStyles::kNameMax);
    }
    if (name.isEmpty()) {
        const QString msg = QStringLiteral("Type a tag.");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const QString phrase = m_nameEditBackup;
    const int caret = m_borrowCaret;
    m_borrowed = BorrowedPhrase::None;
    m_buffer.load(phrase);
    m_buffer.setCaret(caret);
    m_buffer.insertTag(name);
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();

    int index = -1;
    for (int i = 0; i < m_settings.savedSpeechTags.size(); ++i) {
        if (AppSettings::normalizeSpeechTag(m_settings.savedSpeechTags.at(i).name)
                .compare(name, Qt::CaseInsensitive)
            == 0) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        AppSettings::SavedSpeechTag tag;
        tag.name = name;
        m_settings.savedSpeechTags.push_back(tag);
        while (m_settings.savedSpeechTags.size() > 24) {
            m_settings.savedSpeechTags.removeFirst();
        }
        index = m_settings.savedSpeechTags.size() - 1;
    }
    m_tagPage = qMax(0, index) / kTagPage;
    apply();
    showComposeLayer(7);
    notify(QStringLiteral("Inserted"));
    return true;
}

void ComposeUi::cancelTagName()
{
    if (m_borrowed != BorrowedPhrase::Tag) {
        return;
    }
    cancelBorrowedPhrase();
}

void ComposeUi::stampTagLayer(PageDocument& doc) const
{
    PageGrid* grid = doc.findGrid(QStringLiteral("keys_tags"));
    if (!grid) {
        return;
    }
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor surface = theme.defaultCell();
    const QVector<AppSettings::SavedSpeechTag> tags = m_settings.savedSpeechTags;
    const int pages = qMax(1, (tags.size() + kTagPage - 1) / kTagPage);
    const int page = qBound(0, m_tagPage, pages - 1);
    const int start = page * kTagPage;

    grid->rows = kTagBodyRows + 1;
    grid->columns = kTagCols;
    grid->cells.clear();
    for (int slot = 0; slot < kTagPage; ++slot) {
        const int row = slot / kTagCols;
        const int col = slot % kTagCols;
        const int index = start + slot;
        if (index >= tags.size()) {
            grid->cells.push_back(cell(QStringLiteral("tag_slot_%1").arg(slot), {}, row, col, {},
                                       surface, 1, QStringLiteral("label")));
            continue;
        }
        const QString name = AppSettings::normalizeSpeechTag(tags.at(index).name);
        grid->cells.push_back(cell(QStringLiteral("tag_slot_%1").arg(slot), name, row, col,
                                   QStringLiteral("compose.insertTag.%1").arg(encodeTag(name)),
                                   accent));
    }
    const bool canPrev = page > 0;
    const bool canNext = page + 1 < pages;
    const int nav = kTagBodyRows;
    grid->cells.push_back(cell(QStringLiteral("tag_prev"), QStringLiteral("Prev"), nav, 0,
                               canPrev ? QStringLiteral("compose.tagPage.-1") : QString(), surface,
                               1, canPrev ? QString() : QStringLiteral("label")));
    grid->cells.push_back(cell(QStringLiteral("tag_next"), QStringLiteral("Next"), nav, 1,
                               canNext ? QStringLiteral("compose.tagPage.1") : QString(), surface,
                               1, canNext ? QString() : QStringLiteral("label")));
    PageCell pagesCell =
        cell(QStringLiteral("tag_page"),
             pages > 1 ? QStringLiteral("%1/%2").arg(page + 1).arg(pages) : QString(), nav, 2, {},
             surface, 2, QStringLiteral("label"));
    grid->cells.push_back(std::move(pagesCell));
    grid->cells.push_back(cell(QStringLiteral("tag_add"), QStringLiteral("Add"), nav, 4,
                               QStringLiteral("compose.beginTagName"), surface));
    PageCell back = cell(QStringLiteral("tag_back"), QStringLiteral("Back"), nav, 5, {}, surface);
    back.actions.push_back(showLayerAction(5));
    grid->cells.push_back(std::move(back));
}

} // namespace gazer
