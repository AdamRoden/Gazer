#include "app/ComposeUi.h"

#include "app/ComposeUiInternal.h"
#include "layout/PageSession.h"

namespace gazer {

ComposeUi::SearchSheet ComposeUi::activeSearchSheet() const
{
    if (capturingPhrase() || m_iconKeyboard) {
        return SearchSheet::None;
    }
    const QString top = m_pages.topPageId();
    if (top == kVoicesLiveId && m_voiceSearch) {
        return SearchSheet::Voice;
    }
    if (top == kHistoryLiveId && m_historySearch) {
        return SearchSheet::History;
    }
    if (top == kStylesLiveId && m_styleSearch) {
        return SearchSheet::Style;
    }
    if (top == kTagsLiveId && m_tagSearch) {
        return SearchSheet::Tag;
    }
    if (top == kRailLiveId && m_railSearch) {
        return SearchSheet::Rail;
    }
    return SearchSheet::None;
}

QString* ComposeUi::searchQuery(SearchSheet sheet)
{
    switch (sheet) {
    case SearchSheet::Voice:
        return &m_voiceQuery;
    case SearchSheet::History:
        return &m_historyQuery;
    case SearchSheet::Style:
        return &m_styleQuery;
    case SearchSheet::Tag:
        return &m_tagQuery;
    case SearchSheet::Rail:
        return &m_railQuery;
    case SearchSheet::None:
        break;
    }
    return nullptr;
}

void ComposeUi::onSearchEdited(SearchSheet sheet)
{
    switch (sheet) {
    case SearchSheet::Voice:
        m_voicePage = 0;
        rebuildVoices();
        break;
    case SearchSheet::History:
        m_historyPage = 0;
        m_historyClearArm = false;
        rebuildHistory();
        break;
    case SearchSheet::Style:
        m_stylePage = 0;
        rebuildStyles();
        break;
    case SearchSheet::Tag:
        m_tagPage = 0;
        rebuildTags();
        break;
    case SearchSheet::Rail:
        m_railPage = 0;
        rebuildRailAdd();
        break;
    case SearchSheet::None:
        break;
    }
}

bool ComposeUi::handleSearchSend(const PageAction& action)
{
    const SearchSheet sheet = activeSearchSheet();
    QString* query = searchQuery(sheet);
    if (!query) {
        return false;
    }
    const QString add = compose_detail::queryInsertion(action, nullptr);
    if (compose_detail::appendQuery(*query, add)) {
        onSearchEdited(sheet);
    }
    return true;
}

bool ComposeUi::handleSearchCommand(const QString& command)
{
    const SearchSheet sheet = activeSearchSheet();
    QString* query = searchQuery(sheet);
    if (!query) {
        return false;
    }
    using compose_detail::QueryCommand;
    switch (compose_detail::editQuery(*query, command, nullptr)) {
    case QueryCommand::Edited:
        onSearchEdited(sheet);
        break;
    case QueryCommand::Enter:
        break;
    case QueryCommand::Ignored:
        break;
    }
    return true;
}

bool ComposeUi::handleSearchEscape()
{
    const SearchSheet sheet = activeSearchSheet();
    if (QString* query = searchQuery(sheet)) {
        if (!query->isEmpty()) {
            query->clear();
            onSearchEdited(sheet);
            return true;
        }
    }
    const QString top = m_pages.topPageId();
    if (top == kVoicesLiveId && m_langBrowse) {
        m_langBrowse = false;
        m_voicePage = 0;
        rebuildVoices();
        return true;
    }
    if (top == kVoicesLiveId || top == kHistoryLiveId || top == kStylesLiveId || top == kTagsLiveId
        || top == kRailLiveId) {
        if (top == kVoicesLiveId) {
            clearVoiceQuery();
        }
        m_pages.closePage(top);
        return true;
    }
    return false;
}

} // namespace gazer
