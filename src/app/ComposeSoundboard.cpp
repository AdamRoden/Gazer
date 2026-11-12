#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenRequest.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechHistory.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QColor>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVector>

namespace gazer {

using SettingsPageBuild::cell;
using compose_detail::ChoiceSlot;
using compose_detail::commandAction;
using compose_detail::fillChoiceGrid;
using compose_detail::paintOutline;
using compose_detail::parseColor;
using compose_detail::stampEditableGrid;
using compose_detail::trackPx;

QString ComposeUi::activeTopicId() const
{
    return m_board.activeTopicId();
}

void ComposeUi::rebuildBoard()
{
    const PageDocument live = m_pages.attachedCopy(QString(kPageId));
    PageDocument doc = m_composeAuthored.isValid() ? m_composeAuthored : live;
    if (!doc.isValid()) {
        return;
    }
    if (live.isValid()) {
        doc.showLayers = live.showLayers;
    }
    fillSoundboard(doc);
    QString err;
    (void)m_pages.attachDocument(std::move(doc), &err, false, false);
    prefetchBoardIcons();
    refresh();
    m_pages.gateHover(QString(kPageId));
    rebuildForm();
}

PageDim ComposeUi::speakBoardWidth() const
{
    if (!m_composeAuthored.grids.isEmpty() && m_composeAuthored.grids[0].size.x.isSet()) {
        return m_composeAuthored.grids[0].size.x;
    }
    return SettingsPageBuild::defaultSpeakBoardWidth();
}

void ComposeUi::fillSoundboard(PageDocument& doc) const
{
    stampEditKeys(doc);
    if (PageCell* pin = doc.findCell(QStringLiteral("cmd_pin"))) {
        pin->label = assignMode() ? QStringLiteral("Cancel") : QStringLiteral("Pin");
        pin->actions.clear();
        pin->actions.push_back(
            commandAction(assignMode() ? QStringLiteral("compose.cancelAssign")
                                       : QStringLiteral("compose.pin")));
        pin->activeState = QStringLiteral("compose.assignMode");
    }
    if (PageCell* mode = doc.findCell(QStringLiteral("rail_mode"))) {
        mode->label = m_showFavorites ? QStringLiteral("Topics") : QStringLiteral("Favorites");
    }
    stampComposerChrome(doc);
    fillTopicsMode(doc);
}

void ComposeUi::fillTopicsMode(PageDocument& doc) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const bool editing = editMode();

    if (m_showFavorites) {
        fillRail(doc);
    } else {
        QVector<ChoiceSlot> topics;
        const auto& list = m_board.topics();
        const int n = qMin(8, int(list.size()));
        topics.reserve(n);
        for (int i = 0; i < n; ++i) {
            const SoundboardTopic& t = list[i];
            ChoiceSlot s;
            s.label = t.name;
            s.icon = t.icon;
            s.color = t.color;
            s.useCommand = QStringLiteral("soundboard.topic.%1").arg(t.id);
            s.editCommand = QStringLiteral("soundboard.editTopic.%1").arg(t.id);
            s.activeState = QStringLiteral("soundboard.topic.%1").arg(t.id);
            s.selected = editing && m_nameEdit == NameEditKind::Topic && t.id == m_editId;
            s.on = s.selected || (!editing && t.id == m_board.activeTopicId());
            topics.push_back(s);
        }
        fillChoiceGrid(doc.findGrid(QStringLiteral("rail")), 8, 1, QStringLiteral("topic_"), topics,
                       editing, QStringLiteral("soundboard.newTopic"), surface, accent, value);
    }

    PageGrid* grid = doc.findGrid(QStringLiteral("soundboard"));
    stampEditableGrid(grid, editing, accent);
    const SoundboardTopic* topic = m_board.activeTopic();
    if (!grid || !topic) {
        return;
    }
    grid->cells.clear();
    grid->columns = topic->gridCols;
    int rows = topic->gridRows;
    if (assignMode() && rows < SoundboardStore::kMaxRows) {
        ++rows;
    }
    grid->rows = rows;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < topic->gridCols; ++c) {
            PageCell cell;
            cell.id = QStringLiteral("sb_%1_%2").arg(r).arg(c);
            cell.row = r;
            cell.col = c;
            const SoundboardButton* b = m_board.buttonAt(*topic, r, c);
            if (b) {
                cell.label = b->label;
                cell.icon = b->icon;
                const bool selected = editing && m_nameEdit == NameEditKind::Pin && b->id == m_editId;
                const auto plan = m_board.planPlay(*b);
                const QColor ink = parseColor(b->color, surface);
                if (selected) {
                    cell.style.background = accent;
                    cell.style.foreground = ThemeColors::contrastOn(accent);
                    cell.activeState = QStringLiteral("compose.editMode");
                } else if (plan.kind == SoundboardStore::PlayPlan::Kind::Utterance
                           || plan.kind == SoundboardStore::PlayPlan::Kind::Source) {
                    paintOutline(cell, ink);
                } else {
                    cell.style.background = ink;
                    cell.style.foreground = ThemeColors::contrastOn(ink);
                }
                if (assignMode()) {
                    cell.actions.push_back(
                        commandAction(QStringLiteral("soundboard.assign.%1").arg(cell.id)));
                } else if (editing) {
                    cell.actions.push_back(
                        commandAction(QStringLiteral("soundboard.edit.%1").arg(b->id)));
                } else {
                    cell.actions.push_back(
                        commandAction(QStringLiteral("soundboard.play.%1").arg(b->id)));
                }
            } else if (assignMode()) {
                cell.label = QStringLiteral("+");
                cell.style.background = value;
                cell.actions.push_back(
                    commandAction(QStringLiteral("soundboard.assign.%1").arg(cell.id)));
            } else {
                cell.role = QStringLiteral("label");
                cell.style.background = value;
            }
            grid->cells.push_back(cell);
        }
    }
    if (rows <= 0) {
        grid->rows = 1;
        grid->rowTracks = {PageTrackSize::starWeight()};
    } else {
        grid->rowTracks.clear();
        for (int r = 0; r < rows; ++r) {
            grid->rowTracks.push_back(trackPx(80));
        }
        grid->rowTracks.push_back(PageTrackSize::starWeight());
        grid->rows = rows + 1;
    }
}

bool ComposeUi::pin(QString* error)
{
    if (borrowingPhrase()) {
        return saveBorrowedPhrase(error);
    }
    if (nameEditing()) {
        return saveNameEdit(error);
    }
    const QString text = m_buffer.text().trimmed();
    if (text.isEmpty()) {
        return true;
    }
    endIconPick();
    m_interaction = Interaction::Assign;
    rebuildBoard();
    notify(QStringLiteral("Dwell a cell to save"));
    return true;
}

void ComposeUi::cancelAssign()
{
    if (borrowingPhrase()) {
        cancelBorrowedPhrase();
        return;
    }
    if (nameEditing()) {
        cancelNameEdit();
        return;
    }
    const bool any = assignMode() || editMode();
    m_interaction = Interaction::Use;
    endIconPick();
    if (any) {
        rebuildBoard();
    }
}

bool ComposeUi::editPins()
{
    if (borrowingPhrase()) {
        return true;
    }
    if (editMode()) {
        cancelAssign();
        return true;
    }
    m_interaction = Interaction::Edit;
    rebuildBoard();
    notify(QStringLiteral("Dwell a button to edit"));
    return true;
}

bool ComposeUi::editShown()
{
    if (borrowingPhrase() || nameEditing()) {
        return true;
    }
    const QString top = m_pages.topPageId();
    if (top == kStylesLiveId) {
        if (m_styleEditor) {
            closeStyleEditor();
        } else {
            openStyleEditor();
        }
        m_pages.refreshDecorated();
        return true;
    }
    if (top == kTagsLiveId) {
        if (m_tagEditor) {
            closeTagEditor();
        } else {
            openTagEditor();
        }
        m_pages.refreshDecorated();
        return true;
    }
    if (top.isEmpty() || top == kPageId) {
        return editPins();
    }
    return true;
}

void ComposeUi::stampEditKeys(PageDocument& doc) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QString top = m_pages.topPageId();
    const bool naming = (top == kStylesLiveId && m_borrowed == BorrowedPhrase::Style)
                        || (top == kTagsLiveId && m_borrowed == BorrowedPhrase::Tag);
    bool done = false;
    if (!m_iconKeyboard && !naming) {
        if (top == kStylesLiveId) {
            done = m_styleEditor;
        } else if (top == kTagsLiveId) {
            done = m_tagEditor;
        } else if (top.isEmpty() || top == kPageId) {
            done = editMode();
        }
    }
    const QStringList ids = {QStringLiteral("edit"), QStringLiteral("edit_s"),
                             QStringLiteral("edit_sym"), QStringLiteral("edit_ss")};
    for (const QString& id : ids) {
        PageCell* key = doc.findCell(id);
        if (!key) {
            continue;
        }
        key->actions.clear();
        key->activeState.clear();
        key->style.foreground.reset();
        key->style.background = QColor();
        if (m_iconKeyboard) {
            key->label = QStringLiteral("Esc");
            key->icon.clear();
            key->actions.push_back(commandAction(QStringLiteral("escape")));
            key->style.background.token = QStringLiteral("bg90");
            continue;
        }
        if (naming) {
            key->label = QStringLiteral("Cancel");
            key->icon.clear();
            key->actions.push_back(commandAction(QStringLiteral("compose.cancelName")));
            key->style.background.token = QString();
            key->style.background = theme.defaultCell();
            continue;
        }
        key->label = done ? QStringLiteral("Done") : QStringLiteral("Edit");
        key->icon = done ? QStringLiteral("check") : QStringLiteral("edit");
        key->actions.push_back(commandAction(QStringLiteral("compose.editShown")));
        if (done) {
            key->style.background = accent;
            key->style.foreground = ThemeColors::contrastOn(accent);
        } else {
            key->style.background.token = QStringLiteral("bg90");
        }
    }
}

bool ComposeUi::playSoundboard(const QString& buttonId, QString* error)
{
    const SoundboardButton* b = m_board.findButton(buttonId);
    if (!b) {
        return false;
    }
    const auto plan = m_board.planPlay(*b);
    switch (plan.kind) {
    case SoundboardStore::PlayPlan::Kind::Utterance:
        m_speech.speak(plan.text, SpeakKind::Composed);
        return true;
    case SoundboardStore::PlayPlan::Kind::Clip:
        if (m_speech.playFile(plan.clipPath)) {
            return true;
        }
        if (!b->sourceText.trimmed().isEmpty()) {
            m_speech.speak(b->sourceText, SpeakKind::Composed);
            return true;
        }
        if (error) {
            *error = QStringLiteral("Clip play failed");
        }
        return false;
    case SoundboardStore::PlayPlan::Kind::Source:
        m_speech.speak(plan.text, SpeakKind::Composed);
        return true;
    case SoundboardStore::PlayPlan::Kind::None:
        return true;
    }
    return true;
}

bool ComposeUi::assignSoundboard(const QString& cellOrButtonId, QString* error)
{
    if (!assignMode()) {
        return playSoundboard(cellOrButtonId, error);
    }
    int row = -1;
    int col = -1;
    static const QRegularExpression re(QStringLiteral("^sb_(\\d+)_(\\d+)$"));
    const auto m = re.match(cellOrButtonId);
    if (m.hasMatch()) {
        row = m.captured(1).toInt();
        col = m.captured(2).toInt();
    } else if (const SoundboardButton* b = m_board.findButton(cellOrButtonId)) {
        row = b->row;
        col = b->col;
    }
    if (row < 0 || col < 0) {
        if (error) {
            *error = QStringLiteral("Unknown cell");
        }
        return false;
    }
    const QString raw = m_buffer.text().trimmed();
    if (raw.isEmpty()) {
        const QString msg = QStringLiteral("Type something first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const QString source = ElevenRequest::stripInlineTags(raw);
    SoundboardButton seed;
    seed.label = ellipsis(source.isEmpty() ? raw : source, 24);
    seed.sourceText = source.isEmpty() ? raw : source;
    const SpeechEngine::LastClip clip = m_speech.lastClip();
    const bool clipMatches = !clip.path.isEmpty() && QFileInfo::exists(clip.path)
                             && clip.phrase == source;
    if (clipMatches) {
        QString clipErr;
        seed.clipId = m_board.importClip(clip.path, &clipErr, &m_history);
        if (seed.clipId.isEmpty()) {
            seed.utteranceText = raw;
            notify(clipErr.isEmpty() ? QStringLiteral("Saved as live text") : clipErr);
        }
    } else {
        seed.utteranceText = raw;
    }
    QString err;
    if (!m_board.assignCell(row, col, seed, &err)) {
        notify(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    m_interaction = Interaction::Use;
    rebuildBoard();
    return true;
}

bool ComposeUi::setTopic(const QString& topicId, QString* error)
{
    if (!m_board.setActiveTopic(topicId)) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    m_interaction = Interaction::Use;
    endIconPick();
    rebuildBoard();
    return true;
}

bool ComposeUi::newTopic(QString* error)
{
    if (m_board.topics().size() >= 8) {
        const QString msg = QStringLiteral("Topic row is full");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (!m_board.newTopic(error)) {
        return false;
    }
    return openTopicEdit(m_board.activeTopicId(), error);
}

bool ComposeUi::loadStarters(QString* error)
{
    if (!m_board.loadStarters(error)) {
        return false;
    }
    rebuildBoard();
    return true;
}

void ComposeUi::setBoardNote(const QString& text, bool keep)
{
    m_boardNote = text;
    m_boardNoteKeep = keep && !text.isEmpty();
    notify(text);
}

void ComposeUi::dismissBoardNote()
{
    m_boardNote.clear();
    m_boardNoteKeep = false;
}

void ComposeUi::commitEdit()
{
    dismissBoardNote();
    refresh();
    if (nameEditing()) {
        rebuildForm();
    }
    if (m_borrowed == BorrowedPhrase::Style) {
        rebuildStyles();
    } else if (m_borrowed == BorrowedPhrase::Tag) {
        rebuildTags();
    }
}

bool ComposeUi::importPending(bool replace, QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (m_pendingImport.isEmpty()) {
        const QString msg = QStringLiteral("Dwell a JSON file first");
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        refresh();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(m_pendingImport, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        m_pendingImport.clear();
        m_pendingImportName.clear();
        m_pendingImportPath.clear();
        const QString msg = QStringLiteral("Invalid file");
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        refresh();
        return false;
    }
    const QVector<SoundboardTopic> backupTopics = m_board.topics();
    const QString backupActive = m_board.activeTopicId();
    SoundboardStore::WorkspaceImportedSettings imported;
    QString err;
    if (!m_board.importWorkspace(json.object(),
                                 replace ? SoundboardStore::WorkspaceMode::Replace
                                         : SoundboardStore::WorkspaceMode::Merge,
                                 &imported, &err)) {
        const QString msg = err.isEmpty() ? QStringLiteral("Import failed") : err;
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        refresh();
        return false;
    }
    if (!m_board.save(&err)) {
        m_board.restoreTopics(backupTopics, backupActive);
        m_board.evictUnreferencedClips();
        const QString msg = QStringLiteral("Could not save boards");
        setBoardNote(msg);
        if (error) {
            *error = msg;
        }
        refresh();
        return false;
    }
    if (replace) {
        applyImportedSpeech(imported);
    }
    setBoardNote(replace ? QStringLiteral("Boards replaced") : QStringLiteral("Boards merged"), true);
    rebuildBoard();
    return true;
}

void ComposeUi::applyImportedSpeech(const SoundboardStore::WorkspaceImportedSettings& imported)
{
    if (imported.hasModel) {
        m_settings.speechModel = imported.model;
    }
    if (imported.hasSpeed) {
        m_settings.speechSpeed = imported.speed;
    }
    if (imported.hasPitch) {
        m_settings.speechPitch = imported.pitch;
    }
    if (imported.hasVolume) {
        m_settings.speechVolume = imported.volume;
    }
    apply();
}

} // namespace gazer
