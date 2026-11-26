#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeNameSheet.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QUrl>
#include <QVector>
#include <QtGlobal>

namespace gazer {

using compose_detail::aboveKeyboardHeight;
using compose_detail::kNameCols;
using compose_detail::buildNameSheet;
using compose_detail::nameListRows;
using compose_detail::namePageFor;
using compose_detail::NameEntry;
using compose_detail::NameSheet;

namespace {

QString encodeTag(const QString& name)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(name.toUtf8()));
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
    const BorrowedPhrase kind = m_borrowed;
    endBorrowedPhrase();
    rebuildBoard();
    if (kind == BorrowedPhrase::Style) {
        m_styleSearch = true;
        rebuildStyles();
    } else {
        m_tagSearch = true;
        rebuildTags();
    }
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
    if (hasLive(kTagsLiveId)) {
        m_pages.closePage(QString(kTagsLiveId));
    }
}

void ComposeUi::pageTags(int delta)
{
    if (delta == 0 || !hasLive(kTagsLiveId)) {
        return;
    }
    m_tagPage += delta * kNameCols;
    rebuildTags();
}

void ComposeUi::tagsGoto(int offset)
{
    m_tagPage = offset;
    rebuildTags();
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
    m_tagSearch = true;
    rebuildBoard();
    if (!hasLive(kTagsLiveId)) {
        QString err;
        (void)presentLive(QString(kTagsLiveId), buildTagsDocument(), &err);
    } else {
        rebuildTags();
    }
    if (isOpen()) {
        showComposeLayer(1);
    }
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
    for (const AppSettings::SavedSpeechTag& tag : m_settings.savedSpeechTags) {
        if (AppSettings::normalizeSpeechTag(tag.name).compare(name, Qt::CaseInsensitive) == 0) {
            const QString msg = QStringLiteral("Already in the list.");
            notify(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
    }
    const QString phrase = m_nameEditBackup;
    const int caret = m_borrowCaret;
    m_borrowed = BorrowedPhrase::None;
    m_buffer.load(phrase);
    m_buffer.setCaret(caret);
    if (m_typing) {
        m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
    }
    m_nameEditBackup.clear();

    AppSettings::SavedSpeechTag tag;
    tag.name = name;
    m_settings.savedSpeechTags.push_back(tag);
    while (m_settings.savedSpeechTags.size() > 24) {
        m_settings.savedSpeechTags.removeFirst();
    }
    int shown = 0;
    for (const AppSettings::SavedSpeechTag& saved : m_settings.savedSpeechTags) {
        if (!AppSettings::normalizeSpeechTag(saved.name).isEmpty()) {
            ++shown;
        }
    }
    m_tagPage = namePageFor(qMax(0, shown - 1), shown, nameListRows(m_composeAuthored, false));
    m_tagSearch = true;
    m_tagQuery.clear();
    apply();
    notify(QStringLiteral("Added"));
    return true;
}

void ComposeUi::toggleTagFavorite(const QString& encoded)
{
    const QString name =
        AppSettings::normalizeSpeechTag(QUrl::fromPercentEncoding(encoded.toUtf8()));
    if (name.isEmpty()) {
        return;
    }
    bool found = false;
    for (AppSettings::SavedSpeechTag& tag : m_settings.savedSpeechTags) {
        if (AppSettings::normalizeSpeechTag(tag.name).compare(name, Qt::CaseInsensitive) == 0) {
            tag.favorite = !tag.favorite;
            found = true;
        }
    }
    if (!found) {
        return;
    }
    apply();
}

void ComposeUi::trashTag(const QString& encoded)
{
    const QString name =
        AppSettings::normalizeSpeechTag(QUrl::fromPercentEncoding(encoded.toUtf8()));
    if (name.isEmpty()) {
        return;
    }
    bool removed = false;
    for (int i = m_settings.savedSpeechTags.size() - 1; i >= 0; --i) {
        if (AppSettings::normalizeSpeechTag(m_settings.savedSpeechTags.at(i).name)
                .compare(name, Qt::CaseInsensitive)
            == 0) {
            m_settings.savedSpeechTags.removeAt(i);
            removed = true;
        }
    }
    if (!removed) {
        return;
    }
    apply();
}

void ComposeUi::cancelTagName()
{
    if (m_borrowed != BorrowedPhrase::Tag) {
        return;
    }
    cancelBorrowedPhrase();
}

bool ComposeUi::openTags(QString* error)
{
    m_tagSearch = true;
    m_tagQuery.clear();
    m_tagPage = 0;
    closeComposeSheets(kTagsLiveId);
    const bool ok = presentLive(QString(kTagsLiveId), buildTagsDocument(), error);
    if (ok && isOpen()) {
        showComposeLayer(1);
    }
    return ok;
}

void ComposeUi::rebuildTags()
{
    if (!hasLive(kTagsLiveId)) {
        return;
    }
    QString err;
    PageDocument doc = buildTagsDocument();
    doc.id = QString(kTagsLiveId);
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
}

PageDocument ComposeUi::buildTagsDocument()
{
    const bool naming = m_borrowed == BorrowedPhrase::Tag;
    const QString query = m_tagQuery.trimmed();
    struct Row {
        QString name;
        bool favorite = false;
    };
    QVector<Row> favorites;
    QVector<Row> rest;
    for (const AppSettings::SavedSpeechTag& tag : m_settings.savedSpeechTags) {
        const QString name = AppSettings::normalizeSpeechTag(tag.name);
        if (name.isEmpty()) {
            continue;
        }
        if (!naming && !query.isEmpty() && !name.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        Row row;
        row.name = name;
        row.favorite = tag.favorite;
        if (tag.favorite) {
            favorites.push_back(row);
        } else {
            rest.push_back(row);
        }
    }

    NameSheet spec;
    spec.id = QString(kTagsLiveId);
    spec.name = QStringLiteral("Tags");
    spec.title = naming ? QStringLiteral("New tag") : QStringLiteral("Tags");
    spec.prefix = QStringLiteral("tag");
    spec.naming = naming;
    spec.page = &m_tagPage;
    spec.pageUp = QStringLiteral("compose.tagPage.-1");
    spec.pageDown = QStringLiteral("compose.tagPage.1");
    spec.searchId = QStringLiteral("tag_search");
    spec.placeholder = naming ? QStringLiteral("Tag name") : QStringLiteral("Search tags");
    spec.query = naming ? m_buffer.text() : m_tagQuery;
    spec.clearCommand = naming ? QStringLiteral("compose.clear") : QStringLiteral("compose.clearSearch");
    spec.addCommand = QStringLiteral("compose.beginTagName");
    spec.caret = m_buffer.caret();
    spec.stampCaret = naming;
    const auto addTag = [&](const Row& row) {
        const QString encoded = encodeTag(row.name);
        NameEntry entry;
        entry.label = row.name;
        entry.command = QStringLiteral("compose.insertTag.%1").arg(encoded);
        entry.favCommand = QStringLiteral("compose.favTag.%1").arg(encoded);
        entry.trashCommand = QStringLiteral("compose.trashTag.%1").arg(encoded);
        entry.favorite = row.favorite;
        spec.entries.push_back(std::move(entry));
    };
    for (const Row& row : favorites) {
        addTag(row);
    }
    for (const Row& row : rest) {
        addTag(row);
    }
    return buildNameSheet(spec, m_composeAuthored, m_settings.resolvedTheme(),
                          aboveKeyboardHeight(m_composeAuthored), speakBoardWidth());
}

} // namespace gazer
