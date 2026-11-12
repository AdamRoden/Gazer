#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
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
using compose_detail::paintSurface;
using compose_detail::overlayBody;
using compose_detail::overlayList;
using compose_detail::overlayScrollbar;
using compose_detail::searchField;
using compose_detail::trackPx;

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
    if (kind == BorrowedPhrase::Style) {
        m_styleEditor = true;
    } else if (kind == BorrowedPhrase::Tag) {
        m_tagEditor = true;
    }
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
    m_tagPage += delta * kTileCols;
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
    m_tagEditor = true;
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
    m_tagEditor = true;
    const int count = m_settings.savedSpeechTags.size();
    m_tagPage = qBound(0, count - 1, qMax(0, count + 1 - kTilePage));
    m_tagSearch = true;
    m_tagQuery.clear();
    apply();
    notify(QStringLiteral("Added"));
    return true;
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
    const int maxOff = qMax(0, m_settings.savedSpeechTags.size() + 1 - kTilePage);
    if (m_tagPage > maxOff) {
        m_tagPage = maxOff;
    }
    apply();
}

void ComposeUi::openTagEditor()
{
    m_tagEditor = true;
    m_tagPage = 0;
    m_tagSearch = true;
    if (!hasLive(kTagsLiveId)) {
        if (isOpen()) {
            showComposeLayer(1);
        }
        QString err;
        (void)presentLive(QString(kTagsLiveId), buildTagsDocument(), &err);
        return;
    }
    rebuildTags();
}

void ComposeUi::closeTagEditor()
{
    m_tagEditor = false;
    m_tagPage = 0;
    rebuildTags();
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
    m_tagEditor = false;
    m_tagSearch = true;
    m_tagQuery.clear();
    m_tagPage = 0;
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
    PageDocument doc;
    doc.id = QString(kTagsLiveId);
    doc.name = QStringLiteral("Tags");
    const ThemeColors theme = m_settings.resolvedTheme();
    const bool naming = m_borrowed == BorrowedPhrase::Tag;
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
    const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);

    grid.subGrids.push_back(compose_detail::speakTitleBar(
        naming ? QStringLiteral("New tag")
               : (m_tagEditor ? QStringLiteral("Edit tags") : QStringLiteral("Tags")),
        naming, key));

    if (naming) {
        PageCell field = searchField(QStringLiteral("tag_search"), m_buffer.text(),
                                     QStringLiteral("Tag name"), 1, 0, 1, key, accent);
        field.caretIndex = m_buffer.text().isEmpty()
                               ? 0
                               : qBound(0, m_buffer.caret(), field.label.size());
        grid.cells.push_back(std::move(field));
    } else {
        grid.cells.push_back(searchField(QStringLiteral("tag_search"), m_tagQuery,
                                         QStringLiteral("Search tags"), 1, 0, 1, key, accent));
    }

    QStringList names;
    names.reserve(m_settings.savedSpeechTags.size());
    for (const AppSettings::SavedSpeechTag& tag : m_settings.savedSpeechTags) {
        const QString name = AppSettings::normalizeSpeechTag(tag.name);
        if (!name.isEmpty()) {
            names.push_back(name);
        }
    }
    if (!naming && !m_tagQuery.trimmed().isEmpty()) {
        QStringList matched;
        const QString query = m_tagQuery.trimmed();
        for (const QString& name : names) {
            if (name.contains(query, Qt::CaseInsensitive)) {
                matched.push_back(name);
            }
        }
        names = matched;
    }
    const int addSlot = m_tagEditor && !naming ? 1 : 0;
    const int total = names.size() + addSlot;
    const int maxOff = qMax(0, total - kTilePage);
    m_tagPage = qBound(0, m_tagPage, maxOff);
    const int start = m_tagPage;

    PageGrid body = overlayBody(2, 1);
    PageGrid list = overlayList(kTileCols, kTileRows);
    list.gapPx = 8;
    for (int slot = 0; slot < kTilePage; ++slot) {
        const int index = start + slot;
        const int tileRow = slot / kTileCols;
        const int tileCol = slot % kTileCols;
        if (index >= total) {
            list.cells.push_back(cell(QStringLiteral("tag_name_%1").arg(slot), {}, tileRow, tileCol,
                                      {}, value, 1, QStringLiteral("label")));
            continue;
        }
        if (addSlot && index == names.size()) {
            PageCell add = cell(QStringLiteral("tag_add"), QStringLiteral("Add"), tileRow, tileCol,
                                QStringLiteral("compose.beginTagName"), key, 1, {}, {},
                                QStringLiteral("add"));
            add.style.radius = PageBox::all(12);
            paintSurface(add, key);
            list.cells.push_back(std::move(add));
            continue;
        }
        const QString& name = names.at(index);
        const QString encoded = encodeTag(name);
        if (!m_tagEditor) {
            PageCell tile = cell(QStringLiteral("tag_name_%1").arg(slot), name, tileRow, tileCol,
                                 QStringLiteral("compose.insertTag.%1").arg(encoded), key);
            tile.style.radius = PageBox::all(12);
            list.cells.push_back(std::move(tile));
            continue;
        }
        PageGrid tile = makeNested(QStringLiteral("tag_tile_%1").arg(slot), tileRow, tileCol, 1, 2,
                                   4);
        tile.columnTracks = {PageTrackSize::starWeight(), trackPx(56)};
        PageCell label = cell(QStringLiteral("tag_name_%1").arg(slot), name, 0, 0, {}, key, 1,
                              QStringLiteral("label"));
        label.style.radius = PageBox::all(12);
        tile.cells.push_back(std::move(label));
        PageCell trash = cell(QStringLiteral("tag_trash_%1").arg(slot), {}, 0, 1,
                              QStringLiteral("compose.trashTag.%1").arg(encoded), QColor(), 1, {},
                              {}, QStringLiteral("delete"));
        paintDanger(trash, danger);
        tile.cells.push_back(std::move(trash));
        list.subGrids.push_back(std::move(tile));
    }
    body.subGrids.push_back(std::move(list));
    body.subGrids.push_back(overlayScrollbar(start, kTilePage, total, key,
                                            QStringLiteral("compose.tagPage.-1"),
                                            QStringLiteral("compose.tagPage.1")));
    grid.subGrids.push_back(std::move(body));

    if (naming) {
        PageGrid foot = makeNested(QStringLiteral("footer"), 3, 0, 1, 2, 8);
        foot.columnTracks = {PageTrackSize::starWeight(1.4), PageTrackSize::starWeight()};
        PageCell save = cell(QStringLiteral("tag_save"), QStringLiteral("Save"), 0, 0,
                             QStringLiteral("compose.saveName"), accent);
        paintSave(save, accent);
        foot.cells.push_back(std::move(save));
        PageCell cancel = cell(QStringLiteral("tag_cancel"), QStringLiteral("Cancel"), 0, 1,
                               QStringLiteral("compose.cancelName"), key);
        paintSurface(cancel, key);
        foot.cells.push_back(std::move(cancel));
        grid.subGrids.push_back(std::move(foot));
    }
    return doc;
}

} // namespace gazer
