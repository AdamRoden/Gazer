#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeNameSheet.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/Theme.h"

#include <QUrl>
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

void ComposeUi::pageStyles(int delta)
{
    if (delta == 0 || !hasLive(kStylesLiveId)) {
        return;
    }
    m_stylePage += delta * kNameCols;
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
        m_editBorder.clear();
        m_editIcon.clear();
        m_buffer.load(QString());
    }
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
    int index = 1;
    for (const QString& style : added.catalog.styles) {
        if (!VoiceStyles::isFavorite(added.catalog, style)) {
            continue;
        }
        if (VoiceStyles::keyOf(style) == VoiceStyles::keyOf(added.name)) {
            break;
        }
        ++index;
    }
    m_stylePage = namePageFor(index, added.catalog.styles.size() + 1,
                              nameListRows(m_composeAuthored, false));
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
    m_styleSearch = true;
    m_styleQuery.clear();
    m_stylePage = 0;
    closeComposeSheets(kStylesLiveId);
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
    const bool naming = m_borrowed == BorrowedPhrase::Style;
    const VoiceStyles::Catalog catalog = styleCatalog();
    const QString active = VoiceStyles::normalizeStyleName(m_settings.speechStyle);
    const QString activeKey = VoiceStyles::keyOf(active);
    const QString query = m_styleQuery.trimmed();

    QStringList favorites;
    QStringList rest;
    for (const QString& name : catalog.styles) {
        if (!naming && !query.isEmpty() && !name.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        if (VoiceStyles::isFavorite(catalog, name)) {
            favorites.push_back(name);
        } else {
            rest.push_back(name);
        }
    }

    NameSheet spec;
    spec.id = QString(kStylesLiveId);
    spec.name = QStringLiteral("Styles");
    spec.title = naming ? QStringLiteral("New style") : QStringLiteral("Style");
    spec.prefix = QStringLiteral("style");
    spec.naming = naming;
    spec.page = &m_stylePage;
    spec.pageUp = QStringLiteral("compose.stylePage.-1");
    spec.pageDown = QStringLiteral("compose.stylePage.1");
    spec.searchId = QStringLiteral("style_search");
    spec.placeholder = naming ? QStringLiteral("Style name") : QStringLiteral("Search styles");
    spec.query = naming ? m_buffer.text() : m_styleQuery;
    spec.clearCommand = naming ? QStringLiteral("compose.clear") : QStringLiteral("compose.clearSearch");
    spec.addCommand = QStringLiteral("compose.beginStyleName");
    spec.caret = m_buffer.caret();
    spec.stampCaret = naming;

    NameEntry none;
    none.label = QStringLiteral("None");
    none.command = QStringLiteral("compose.clearStyle");
    none.selected = active.isEmpty();
    spec.entries.push_back(std::move(none));
    const auto addStyle = [&](const QString& name) {
        const QString encoded = encodeStyleName(name);
        NameEntry entry;
        entry.label = name;
        entry.command = QStringLiteral("compose.applyStyle.%1").arg(encoded);
        entry.favCommand = QStringLiteral("compose.favStyle.%1").arg(encoded);
        entry.trashCommand = QStringLiteral("compose.trashStyle.%1").arg(encoded);
        entry.favorite = VoiceStyles::isFavorite(catalog, name);
        entry.selected = !activeKey.isEmpty() && VoiceStyles::keyOf(name) == activeKey;
        spec.entries.push_back(std::move(entry));
    };
    for (const QString& name : favorites) {
        addStyle(name);
    }
    for (const QString& name : rest) {
        addStyle(name);
    }
    return buildNameSheet(spec, m_composeAuthored, m_settings.resolvedTheme(),
                          aboveKeyboardHeight(m_composeAuthored), speakBoardWidth());
}

} // namespace gazer
