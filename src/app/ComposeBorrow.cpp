#include "app/ComposeUi.h"

#include "app/ComposeUiInternal.h"
#include "layout/PageSession.h"

namespace gazer {

void ComposeUi::setGuestText(std::function<void(const QString&)> insert,
                             std::function<void(const QString&)> command)
{
    m_guestInsert = std::move(insert);
    m_guestCommand = std::move(command);
}

void ComposeUi::setSettingsFileHandlers(
    std::function<bool(const QString& path, QString* error)> importFn,
    std::function<bool(const QString& path, QString* error)> exportFn)
{
    m_settingsImport = std::move(importFn);
    m_settingsExport = std::move(exportFn);
}

bool ComposeUi::keyboardBorrowed() const
{
    return m_guestKeys || settingsFileKeyboard();
}

bool ComposeUi::settingsFileKeyboard() const
{
    return m_fileJob == FileJob::Settings && hasLive(kFilesLiveId);
}

bool ComposeUi::captureLetsCommandThrough(const QString& command) const
{
    return isAllowThroughCommand(command);
}

bool ComposeUi::guestCapture() const
{
    if (!m_guestKeys || editingFileName() || m_iconKeyboard || capturingPhrase()) {
        return false;
    }
    return activeSearchSheet() == SearchSheet::None;
}

bool ComposeUi::handleGuestSend(const PageAction& action)
{
    if (!guestCapture()) {
        return false;
    }
    const QString add = compose_detail::queryInsertion(action, nullptr);
    if (!add.isEmpty() && m_guestInsert) {
        m_guestInsert(add);
    }
    return true;
}

bool ComposeUi::handleGuestCommand(const QString& command)
{
    if (!guestCapture()) {
        return false;
    }
    const QString c = command.trimmed();
    QString forwarded;
    if (c == QLatin1String("backspace") || c == QLatin1String("compose.backspace")) {
        forwarded = QStringLiteral("backspace");
    } else if (c == QLatin1String("space") || c == QLatin1String("enter")) {
        forwarded = c;
    }
    if (!forwarded.isEmpty() && m_guestCommand) {
        m_guestCommand(forwarded);
    }
    return true;
}

void ComposeUi::applyGuestChrome(PageDocument& doc) const
{
    PageGrid* board = doc.findGrid(QStringLiteral("board"));
    const PageGrid* auth = m_composeAuthored.findGrid(QStringLiteral("board"));
    if (!board || !auth) {
        return;
    }
    if (!keyboardBorrowed()) {
        board->anchor = auth->anchor;
        board->offset = auth->offset;
        board->size = auth->size;
        board->rowTracks = auth->rowTracks;
        board->marginPx = auth->marginPx;
        board->gapPx = auth->gapPx;
        return;
    }
    board->anchor = PageAnchor::Bottom;
    board->offset = {};
    board->size.x = auth->size.x;
    board->rowTracks = {PageTrackSize::fromDim(PageDim::pixels(0)), PageTrackSize::starWeight()};
    if (settingsFileKeyboard()) {
        // Same height string the file browser already uses. Margin and gap are
        // zero so the keys grid starts on that shared edge.
        board->marginPx = 0;
        board->gapPx = 0;
        board->size.y = PageDim::expression(
            QStringLiteral("A_ScreenHeight-(%1)").arg(compose_detail::filesOverlayHeight()));
    } else {
        board->marginPx = auth->marginPx;
        board->gapPx = auth->gapPx;
        board->size.y = PageDim::expression(QStringLiteral("0.32*A_ScreenHeight"));
    }
}

void ComposeUi::releaseBorrowedKeyboard()
{
    if (keyboardBorrowed()) {
        if (isOpen()) {
            refresh();
        }
        return;
    }
    if (!m_borrowedCompose) {
        if (isOpen()) {
            refresh();
        }
        return;
    }
    m_borrowedCompose = false;
    if (isOpen()) {
        m_pages.closePage(QString(kPageId));
    }
}

bool ComposeUi::beginGuestKeyboard(QString* error)
{
    if (capturingPhrase() || m_iconKeyboard || activeSearchSheet() != SearchSheet::None) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (!isOpen()) {
        abandonClosedSession();
        m_borrowedCompose = true;
        m_guestKeys = true;
        if (!m_pages.openPage(QString(kPageId), error)) {
            m_guestKeys = false;
            m_borrowedCompose = false;
            return false;
        }
    } else if (!m_guestKeys) {
        m_guestKeys = true;
        refresh();
    }
    showComposeLayer(1);
    return true;
}

void ComposeUi::endGuestKeyboard()
{
    if (!m_guestKeys) {
        return;
    }
    m_guestKeys = false;
    releaseBorrowedKeyboard();
}

bool ComposeUi::openSettingsFiles(bool exportMode, QString* error)
{
    if (capturingPhrase() || m_iconKeyboard || activeSearchSheet() != SearchSheet::None) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (!isOpen()) {
        abandonClosedSession();
        m_borrowedCompose = true;
        if (!m_pages.openPage(QString(kPageId), error)) {
            m_borrowedCompose = false;
            return false;
        }
    }
    if (!openFileMode(exportMode ? FileMode::Export : FileMode::Import, error, FileJob::Settings)) {
        m_fileJob = FileJob::Speak;
        releaseBorrowedKeyboard();
        return false;
    }
    refresh();
    return true;
}

} // namespace gazer
