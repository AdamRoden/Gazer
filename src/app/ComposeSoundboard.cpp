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
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
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
    const QStringList editIds = {QStringLiteral("pin"), QStringLiteral("pin_s"),
                                 QStringLiteral("pin_sym"), QStringLiteral("pin_ss")};
    for (const QString& id : editIds) {
        PageCell* edit = doc.findCell(id);
        if (!edit) {
            continue;
        }
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
            edit->style.background.token = QStringLiteral("bg90");
            edit->style.foreground.reset();
        }
    }
    if (PageCell* pin = doc.findCell(QStringLiteral("cmd_pin"))) {
        pin->label = assignMode() ? QStringLiteral("Cancel") : QStringLiteral("Pin");
        pin->actions.clear();
        pin->actions.push_back(
            commandAction(assignMode() ? QStringLiteral("compose.cancelAssign")
                                       : QStringLiteral("compose.pin")));
        pin->activeState = QStringLiteral("compose.assignMode");
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

bool ComposeUi::pin(QString* error)
{
    if (m_styleNaming) {
        return saveStyleName(error);
    }
    if (nameEditing()) {
        return saveNameEdit(error);
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
    if (m_styleNaming) {
        cancelStyleName();
        return;
    }
    if (nameEditing()) {
        cancelNameEdit();
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
    if (m_styleNaming) {
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

bool ComposeUi::exportBoard(QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(dir).filePath(
        QStringLiteral("aac-workspace-%1.json").arg(QDate::currentDate().toString(Qt::ISODate)));
    const QString path = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("Export boards"), suggested, QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return true;
    }
    SoundboardStore::WorkspaceExportSettings settings;
    settings.model = m_settings.speechModel;
    settings.speed = m_settings.speechSpeed;
    settings.pitch = m_settings.speechPitch;
    settings.volumeStop = qBound(1, qRound(m_settings.speechVolume), 5);
    settings.theme = themeAppearanceIsDark(m_settings.themeAppearance) ? QStringLiteral("dark")
                                                                        : QStringLiteral("light");
    settings.accentColor = m_settings.customPrimaryColor;
    bool omitted = false;
    const QByteArray bytes = m_board.exportWorkspace(settings, &omitted);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        const QString msg = QStringLiteral("Could not write the export");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    notify(omitted ? QStringLiteral(
                         "Audio clips omitted (file too large); phrases export as live text.")
                   : QStringLiteral("Boards exported"));
    return true;
}

bool ComposeUi::importChoose(QString* error)
{
    if (capturingPhrase()) {
        const QString msg = QStringLiteral("Finish the name first");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("Import boards"), dir, QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        const QString msg = QStringLiteral("Could not read the file");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        m_pendingImport.clear();
        m_pendingImportName.clear();
        const QString msg = QStringLiteral("Invalid file");
        notify(msg);
        if (error) {
            *error = msg;
        }
        refresh();
        return false;
    }
    m_pendingImport = json.toJson(QJsonDocument::Compact);
    m_pendingImportName = QFileInfo(path).fileName();
    refresh();
    return true;
}

bool ComposeUi::importMerge(QString* error)
{
    return importPending(false, error);
}

bool ComposeUi::importReplace(QString* error)
{
    return importPending(true, error);
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
        const QString msg = QStringLiteral("Choose a file");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(m_pendingImport, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        m_pendingImport.clear();
        m_pendingImportName.clear();
        const QString msg = QStringLiteral("Invalid file");
        notify(msg);
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
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (!m_board.save(&err)) {
        m_board.restoreTopics(backupTopics, backupActive);
        m_board.evictUnreferencedClips();
        const QString msg = QStringLiteral("Could not save boards");
        notify(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    if (replace) {
        applyImportedSpeech(imported);
    }
    notify(replace ? QStringLiteral("Boards replaced") : QStringLiteral("Boards merged"));
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
