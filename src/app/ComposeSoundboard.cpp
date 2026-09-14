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

#include <QColor>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVector>

namespace gazer {

using compose_detail::ChoiceSlot;
using compose_detail::commandAction;
using compose_detail::fillChoiceGrid;
using compose_detail::parseColor;
using compose_detail::stampEditableGrid;

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
    refresh();
    m_pages.gateHover(QString(kPageId));
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
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor ok(46, 125, 50);
    const QStringList pinIds = {QStringLiteral("pin"), QStringLiteral("pin_s"),
                                QStringLiteral("pin_sym"), QStringLiteral("pin_ss")};
    for (const QString& id : pinIds) {
        PageCell* pin = doc.findCell(id);
        if (!pin) {
            continue;
        }
        if (assignMode()) {
            pin->label = QStringLiteral("Cancel");
        } else if (freestyleMode()) {
            pin->label = QStringLiteral("Save");
        } else {
            pin->label = QStringLiteral("Pin");
        }
        pin->actions.clear();
        pin->actions.push_back(
            commandAction(assignMode() ? QStringLiteral("compose.cancelAssign")
                                       : QStringLiteral("compose.pin")));
        pin->activeState = QStringLiteral("compose.assignMode");
    }
    if (PageCell* edit = doc.findCell(QStringLiteral("editPins"))) {
        edit->label = editMode() ? QStringLiteral("Done") : QStringLiteral("Edit");
        edit->icon = editMode() ? QStringLiteral("check") : QStringLiteral("edit");
        edit->actions.clear();
        edit->actions.push_back(
            commandAction(editMode() ? QStringLiteral("compose.cancelAssign")
                                     : QStringLiteral("compose.editPins")));
        edit->activeState = QStringLiteral("compose.editMode");
        if (editMode()) {
            edit->style.background = ok;
            edit->style.foreground = QColor(255, 255, 255);
        } else {
            edit->style.background = QColor();
            edit->style.foreground = QColor();
        }
    }
    stampComposerChrome(doc);
    if (freestyleMode()) {
        fillFreestyleMode(doc);
    } else {
        fillTopicsMode(doc);
    }
}

void ComposeUi::fillTopicsMode(PageDocument& doc) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const QColor value = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    const bool editing = editMode();

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
    fillChoiceGrid(doc.findGrid(QStringLiteral("topics")), 1, 8, QStringLiteral("topic_"), topics,
                   editing, QStringLiteral("soundboard.newTopic"), surface, accent, value);

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
                const QColor bg = selected ? accent : parseColor(b->color, surface);
                cell.style.background = bg;
                cell.style.foreground = ThemeColors::contrastOn(bg);
                if (selected) {
                    cell.activeState = QStringLiteral("compose.editMode");
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
}

bool ComposeUi::toggleFreestyle(QString* error)
{
    Q_UNUSED(error);
    if (nameEditing()) {
        finishItemEditor(true);
    }
    m_interaction = Interaction::Use;
    closeItemEdit();
    m_boardKind = m_boardKind == BoardKind::Freestyle ? BoardKind::Topics : BoardKind::Freestyle;
    rebuildBoard();
    notify(freestyleMode() ? QStringLiteral("Freestyle") : QStringLiteral("Topics"));
    return true;
}

bool ComposeUi::pin(QString* error)
{
    if (nameEditing()) {
        return saveNameEdit(error);
    }
    if (freestyleMode()) {
        return newVoicePreset(error);
    }
    const QString text = m_buffer.text().trimmed();
    if (text.isEmpty()) {
        return true;
    }
    closeItemEdit();
    m_interaction = Interaction::Assign;
    rebuildBoard();
    notify(QStringLiteral("Dwell a cell to save"));
    return true;
}

void ComposeUi::cancelAssign()
{
    if (nameEditing()) {
        finishItemEditor(true);
        return;
    }
    const bool any = assignMode() || editMode();
    m_interaction = Interaction::Use;
    closeItemEdit();
    if (any) {
        rebuildBoard();
    }
}

bool ComposeUi::editPins()
{
    if (editMode()) {
        cancelAssign();
        return true;
    }
    m_interaction = Interaction::Edit;
    rebuildBoard();
    notify(freestyleMode() ? QStringLiteral("Dwell a voice or tag")
                           : QStringLiteral("Dwell a button to edit"));
    return true;
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
    closeItemEdit();
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

} // namespace gazer
