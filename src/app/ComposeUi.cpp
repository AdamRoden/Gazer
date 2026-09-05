#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"
#include "assist/PhraseService.h"
#include "assist/SpeechEngine.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechHistory.h"
#include "assist/SpeechSecrets.h"
#include "assist/SystemVolume.h"
#include "assist/TtsService.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QObject>
#include <QStringView>
#include <QtGlobal>
#include <QVector>

namespace gazer {

using SettingsPageBuild::cell;
using compose_detail::commandAction;
using compose_detail::parseColor;

ComposeUi::ComposeUi(PageSession& pages, PhraseService& phrases, SpeechEngine& speech,
                     AppSettings& settings, SpeechSecrets& secrets, ElevenClient& eleven,
                     TtsService& tts, SoundboardStore& board, SpeechHistory& history)
    : m_pages(pages)
    , m_phrases(phrases)
    , m_speech(speech)
    , m_settings(settings)
    , m_secrets(secrets)
    , m_eleven(eleven)
    , m_tts(tts)
    , m_board(board)
    , m_history(history)
    , m_systemVolume(std::make_unique<SystemVolume>())
{
    QObject::connect(m_systemVolume.get(), &SystemVolume::changed, &m_pages, [this]() {
        if (isOpen()) {
            refresh();
        }
    });
}

ComposeUi::~ComposeUi() = default;

bool ComposeUi::isCapturing(const QString& sourcePageId) const
{
    return sourcePageId == kPageId || sourcePageId == kVoicesLiveId
           || sourcePageId == kHistoryLiveId || sourcePageId == kItemEditLiveId;
}

bool ComposeUi::isOpen() const
{
    return m_pages.hasPage(QString(kPageId));
}

bool ComposeUi::assignMode() const
{
    return m_interaction == Interaction::Assign;
}

bool ComposeUi::editMode() const
{
    return m_interaction == Interaction::Edit || m_interaction == Interaction::NameEdit;
}

bool ComposeUi::freestyleMode() const
{
    return m_boardKind == BoardKind::Freestyle;
}

bool ComposeUi::nameEditing() const
{
    return m_interaction == Interaction::NameEdit;
}

bool ComposeUi::hasLive(QLatin1String id) const
{
    return m_pages.hasPage(QString(id));
}

bool ComposeUi::isAllowThroughCommand(const QString& name)
{
    return name.startsWith(QLatin1String("compose."))
           || name.startsWith(QLatin1String("speech."))
           || name.startsWith(QLatin1String("soundboard."))
           || name.startsWith(QLatin1String("history."))
           || name.startsWith(QLatin1String("settings.speech."))
           || name == QLatin1String("toggleDwellSuspend")
           || name == QLatin1String("suspendDwell")
           || name == QLatin1String("resumeDwell");
}

bool ComposeUi::tryHandle(const PageAction& a, const QString& sourcePageId)
{
    if (!isCapturing(sourcePageId)) {
        return false;
    }
    if (a.type == PageActionType::Send) {
        handleSend(a);
        return true;
    }
    if (a.type == PageActionType::Command) {
        if (isAllowThroughCommand(a.command)) {
            return false;
        }
        handleCapturedCommand(a.command);
        return true;
    }
    return false;
}

void ComposeUi::apply()
{
    syncActiveVoicePreset();
    if (m_apply) {
        m_apply();
    }
    refreshLiveBoards();
}

void ComposeUi::notify(const QString& msg)
{
    if (m_notify && !msg.isEmpty()) {
        m_notify(msg);
    }
}

void ComposeUi::refresh()
{
    syncClosedOverlays();
    m_pages.refreshDecorated();
    m_pages.refreshActive();
}

void ComposeUi::refreshLiveBoards()
{
    rebuildVoices();
    rebuildHistory();
    rebuildItemEdit();
    refresh();
}

void ComposeUi::syncActiveVoicePreset()
{
    if (m_activeVoicePresetId.isEmpty()) {
        return;
    }
    for (const AppSettings::SavedSpeechVoice& v : m_settings.savedSpeechVoices) {
        if (v.id != m_activeVoicePresetId) {
            continue;
        }
        const QString liveId =
            ElevenRequest::isElevenModel(ElevenRequest::normalizeModelId(v.model))
                ? m_settings.elevenVoiceId.trimmed()
                : m_settings.sapiVoiceToken.trimmed();
        if (v.model == m_settings.speechModel && v.voiceId.trimmed() == liveId
            && qAbs(v.speed - m_settings.speechSpeed) < 0.05
            && qAbs(v.volume - m_settings.speechVolume) < 0.05) {
            return;
        }
        break;
    }
    m_activeVoicePresetId.clear();
}

void ComposeUi::syncFromSettings()
{
    const QString before = m_activeVoicePresetId;
    syncActiveVoicePreset();
    if (before != m_activeVoicePresetId && isOpen() && freestyleMode()) {
        rebuildBoard();
    }
}

void ComposeUi::insertText(QStringView chars)
{
    if (chars.isEmpty()) {
        return;
    }
    m_buffer.insert(chars);
    refresh();
}

void ComposeUi::handleSend(const PageAction& a)
{
    const QString top = m_pages.topPageId();
    if (top != kPageId && !nameEditing()) {
        return;
    }
    const QString key = a.sendKey.trimmed();
    if (key.isEmpty()) {
        return;
    }
    if (key.size() == 1) {
        insertText(key);
        return;
    }
    if (key.compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
        insertText(QStringLiteral(" "));
    }
}

void ComposeUi::handleCapturedCommand(const QString& command)
{
    const QString c = command.trimmed();
    const QString top = m_pages.topPageId();
    if (c == QLatin1String("escape")) {
        const auto st = m_speech.status();
        if (st.busy || st.speaking) {
            stopSpeech();
            return;
        }
        if (nameEditing()) {
            finishItemEditor(true);
            return;
        }
        if ((assignMode() || editMode()) && top == kPageId) {
            cancelAssign();
            return;
        }
        if (top == kItemEditLiveId) {
            finishItemEditor(true);
            return;
        }
        if (top == kVoicesLiveId) {
            m_pages.closePage(QString(kVoicesLiveId));
            return;
        }
        if (top == kHistoryLiveId) {
            m_pages.closePage(QString(kHistoryLiveId));
            return;
        }
        m_pages.closePage(QString(kPageId));
        return;
    }
    if (nameEditing()) {
        if (c == QLatin1String("backspace") || c == QLatin1String("compose.backspace")) {
            backspace();
            return;
        }
        if (c == QLatin1String("space")) {
            insertText(QStringLiteral(" "));
            return;
        }
        if (c == QLatin1String("enter")) {
            QString err;
            saveNameEdit(&err);
        }
        return;
    }
    if (top != kPageId) {
        return;
    }
    if (c == QLatin1String("backspace") || c == QLatin1String("compose.backspace")) {
        backspace();
        return;
    }
    if (c == QLatin1String("space")) {
        insertText(QStringLiteral(" "));
        return;
    }
    if (c == QLatin1String("enter")) {
        QString err;
        speakOrStop(&err);
    }
}

void ComposeUi::abandonClosedSession()
{
    if (nameEditing()) {
        endNameEdit(true);
    } else {
        m_nameEdit = NameEditKind::None;
        m_editId.clear();
        m_nameEditBackup.clear();
    }
    m_interaction = Interaction::Use;
    m_editIcons = false;
    m_editColor.clear();
    m_editIcon.clear();
    closeItemEdit();
}

void ComposeUi::onSessionChanged()
{
    if (isOpen() || hasLive(kItemEditLiveId)) {
        return;
    }
    abandonClosedSession();
}

bool ComposeUi::openCompose(QString* error)
{
    const bool wasOpen = isOpen();
    if (!wasOpen) {
        abandonClosedSession();
    }
    if (!m_pages.openPage(QString(kPageId), error)) {
        return false;
    }
    rebuildBoard();
    return true;
}

bool ComposeUi::speakOrStop(QString* error)
{
    if (nameEditing()) {
        return saveNameEdit(error);
    }
    const auto st = m_speech.status();
    if (st.busy || st.speaking) {
        stopSpeech();
        return true;
    }
    const QString text = m_buffer.text().trimmed();
    if (text.isEmpty()) {
        return true;
    }
    return m_phrases.speak(text, SpeakKind::Composed, error, true);
}

void ComposeUi::stopSpeech()
{
    m_speech.stop();
    refresh();
}

void ComposeUi::clear()
{
    m_buffer.clear();
    refresh();
}

bool ComposeUi::undo()
{
    const bool ok = m_buffer.undo();
    refresh();
    return ok;
}

bool ComposeUi::redo()
{
    const bool ok = m_buffer.redo();
    refresh();
    return ok;
}

void ComposeUi::backspace()
{
    m_buffer.backspace();
    refresh();
}

void ComposeUi::deleteWord()
{
    m_buffer.deleteWord();
    refresh();
}

void ComposeUi::removeVisibleWord(int slot)
{
    m_buffer.removeVisibleWord(slot);
    refresh();
}

void ComposeUi::insertTagAt(int index)
{
    if (nameEditing()) {
        return;
    }
    if (index < 0 || index >= m_settings.savedSpeechTags.size()) {
        return;
    }
    m_buffer.insertTag(m_settings.savedSpeechTags.at(index).name);
    refresh();
}

void ComposeUi::setSpeechModel(const QString& model)
{
    m_settings.speechModel = model;
    m_voicePage = 0;
    apply();
    if (freestyleMode()) {
        rebuildBoard();
    }
}

void ComposeUi::nudgeSpeed(int dir)
{
    if (!m_settings.nudge(QStringLiteral("speechSpeed"), dir)) {
        return;
    }
    apply();
    if (freestyleMode()) {
        rebuildBoard();
    }
}

void ComposeUi::nudgeVolume(int dir)
{
    if (!m_settings.nudge(QStringLiteral("speechVolume"), dir)) {
        return;
    }
    apply();
    if (freestyleMode()) {
        rebuildBoard();
    }
}

void ComposeUi::nudgeSystemVolume(int dir)
{
    if (!m_systemVolume->nudge(dir)) {
        return;
    }
    refresh();
}

QString ComposeUi::ellipsis(const QString& text, int maxChars)
{
    if (text.size() <= maxChars) {
        return text;
    }
    return text.left(qMax(0, maxChars - 1)) + QChar(0x2026);
}

void ComposeUi::decoratePage(PageDocument& doc) const
{
    if (doc.id != kPageId) {
        return;
    }
    if (!doc.findCell(QStringLiteral("topic_0"))) {
        fillSoundboard(doc);
    }
    const auto st = m_speech.status();
    const bool stop = st.busy || st.speaking;
    const bool naming = nameEditing();
    const auto vis = naming ? QVector<ComposeBuffer::Token>{} : m_buffer.visibleTokens();
    const QString phrase = ellipsis(m_buffer.text(), 120);

    auto paint = [&](PageGrid& g, auto& self) -> void {
        for (PageCell& c : g.cells) {
            if (c.id == QLatin1String("phrase")) {
                c.label = naming ? ellipsis(m_buffer.text().trimmed(), 24) : phrase;
            } else if (c.id == QLatin1String("vol_track")) {
                c.label = QStringLiteral("%1%").arg(m_systemVolume->percent());
            } else if (c.id == QLatin1String("page_title")) {
                if (naming) {
                    c.caption = QStringLiteral("Type a name, then Save");
                } else if (editMode() && freestyleMode()) {
                    c.caption = QStringLiteral("Dwell a voice or tag");
                } else if (editMode()) {
                    c.caption = QStringLiteral("Dwell a button or + topic");
                } else if (assignMode()) {
                    c.caption = QStringLiteral("Dwell a cell to save");
                } else if (m_speech.elevenLatched()) {
                    c.caption = QStringLiteral("Using SAPI (network)");
                } else {
                    c.caption.clear();
                }
            } else if (c.id == QLatin1String("speak") && !naming) {
                c.label = stop ? QStringLiteral("Stop") : QStringLiteral("Speak");
                c.icon = stop ? QStringLiteral("Sleep") : QStringLiteral("Speak");
            } else if (c.id == QLatin1String("chip_more")) {
                c.label = !naming && m_buffer.tokens().size() > ComposeBuffer::kVisibleChips
                              ? QStringLiteral("\u22ef")
                              : QString();
            } else if (c.id.startsWith(QLatin1String("chip_"))) {
                bool ok = false;
                const int i = QStringView(c.id).mid(5).toInt(&ok);
                if (naming || !ok) {
                    c.label.clear();
                } else if (i >= 0 && i < vis.size()) {
                    c.label = vis[i].text;
                } else {
                    c.label.clear();
                }
            }
        }
        for (PageGrid& sub : g.subGrids) {
            self(sub, self);
        }
    };
    for (PageGrid& g : doc.grids) {
        paint(g, paint);
    }
}

void ComposeUi::stampComposerChrome(PageDocument& doc) const
{
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor ok(46, 125, 50);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor surface = theme.bgSurface.isValid() ? theme.bgSurface : QColor(40, 40, 44);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    const bool naming = nameEditing();

    const QVector<int> allLayers{1, 2, 3, 4};
    for (const char* gid : {"edit_keys", "phrase", "chips", "speak_keys"}) {
        if (PageGrid* g = doc.findGrid(QLatin1String(gid))) {
            g->layers = allLayers;
        }
    }

    auto setCmd = [](PageCell* c, const QString& cmd) {
        if (!c) {
            return;
        }
        c->actions.clear();
        if (!cmd.isEmpty()) {
            c->actions.push_back(commandAction(cmd));
            if (c->role == QLatin1String("label") || c->role == QLatin1String("display")) {
                c->role.clear();
            }
        } else {
            c->role = QStringLiteral("label");
        }
    };

    if (PageGrid* keys = doc.findGrid(QStringLiteral("edit_keys"))) {
        keys->cells.clear();
        keys->rows = naming ? 2 : 3;
        keys->columns = 1;
        if (naming) {
            PageCell color = cell(QStringLiteral("clear"), QStringLiteral("Color"), 0, 0,
                                  QStringLiteral("compose.editShowColors"),
                                  !m_editIcons ? accent : surface, 1, {}, {},
                                  QStringLiteral("palette"));
            color.activeState = QStringLiteral("compose.editColors");
            keys->cells.push_back(color);
            PageCell image = cell(QStringLiteral("undo"), QStringLiteral("Icon"), 1, 0,
                                  QStringLiteral("compose.editShowIcons"),
                                  m_editIcons ? accent : surface, 1, {}, {},
                                  QStringLiteral("photoCamera"));
            image.activeState = QStringLiteral("compose.editIcons");
            keys->cells.push_back(image);
        } else {
            keys->cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 0, 0,
                                       QStringLiteral("compose.clear"), QColor()));
            keys->cells.push_back(cell(QStringLiteral("undo"), QStringLiteral("Undo"), 1, 0,
                                       QStringLiteral("compose.undo"), QColor(), 1, {}, {},
                                       QStringLiteral("undo")));
            keys->cells.push_back(cell(QStringLiteral("redo"), QStringLiteral("Redo"), 2, 0,
                                       QStringLiteral("compose.redo"), QColor(), 1, {}, {},
                                       QStringLiteral("redo")));
        }
    }

    if (PageGrid* phraseGrid = doc.findGrid(QStringLiteral("phrase"))) {
        phraseGrid->rowSpan = naming ? 2 : 1;
    }
    if (PageGrid* chips = doc.findGrid(QStringLiteral("chips"))) {
        if (naming) {
            chips->row = 0;
            chips->col = 0;
            chips->colSpan = 0;
            chips->rowSpan = 0;
            chips->cells.clear();
        } else {
            chips->row = 1;
            chips->col = 1;
            chips->colSpan = 6;
            chips->rowSpan = 1;
            if (chips->cells.isEmpty()) {
                for (int i = 0; i < ComposeBuffer::kVisibleChips; ++i) {
                    chips->cells.push_back(
                        cell(QStringLiteral("chip_%1").arg(i), {}, 0, i,
                             QStringLiteral("compose.removeWord.%1").arg(i), QColor()));
                }
                PageCell more = cell(QStringLiteral("chip_more"), {}, 0, 12, {}, QColor(), 1,
                                     QStringLiteral("label"));
                chips->cells.push_back(more);
            } else {
                for (int i = 0; i < ComposeBuffer::kVisibleChips; ++i) {
                    if (PageCell* chip = doc.findCell(QStringLiteral("chip_%1").arg(i))) {
                        chip->role.clear();
                        chip->actions.clear();
                        chip->actions.push_back(
                            commandAction(QStringLiteral("compose.removeWord.%1").arg(i)));
                    }
                }
            }
        }
    }

    if (PageCell* phrase = doc.findCell(QStringLiteral("phrase"))) {
        if (naming) {
            const QColor bg = parseColor(m_editColor, surface);
            phrase->label = ellipsis(m_buffer.text().trimmed(), 24);
            phrase->icon = m_editIcon;
            phrase->caption.clear();
            phrase->role = QStringLiteral("display");
            phrase->textStyle = QStringLiteral("title");
            phrase->style.background = bg;
            phrase->style.foreground = ThemeColors::contrastOn(bg);
            phrase->style.borderColor = accent;
            phrase->style.thickness = PageBox::all(3);
            phrase->actions.clear();
        } else {
            phrase->icon.clear();
            phrase->role = QStringLiteral("display");
            phrase->textStyle = QStringLiteral("body");
            phrase->style.background = QColor();
            phrase->style.foreground = QColor();
            phrase->style.borderColor = QColor();
            phrase->style.thickness.reset();
            phrase->actions.clear();
        }
    }

    if (PageCell* speak = doc.findCell(QStringLiteral("speak"))) {
        if (naming) {
            speak->label = QStringLiteral("Save");
            speak->icon = QStringLiteral("YesNoCheck");
            speak->style.background = ok;
            speak->style.foreground = QColor(255, 255, 255);
            setCmd(speak, QStringLiteral("compose.saveName"));
        } else {
            speak->label = QStringLiteral("Speak");
            speak->icon = QStringLiteral("Speak");
            speak->style.background = QColor();
            speak->style.foreground = QColor();
            setCmd(speak, QStringLiteral("compose.speak"));
        }
        speak->activeState = naming ? QString() : QStringLiteral("compose.busy");
    }
    if (PageCell* voices = doc.findCell(QStringLiteral("voices"))) {
        if (naming) {
            voices->label = QStringLiteral("Cancel");
            voices->icon = QStringLiteral("close");
            voices->style.background = surface;
            setCmd(voices, QStringLiteral("compose.cancelName"));
        } else {
            voices->label = QStringLiteral("Voice");
            voices->icon = QStringLiteral("recordVoiceOver");
            voices->style.background = QColor();
            voices->style.foreground = QColor();
            setCmd(voices, QStringLiteral("compose.openVoices"));
        }
    }
    if (PageCell* history = doc.findCell(QStringLiteral("history"))) {
        if (naming && nameEditCanDelete()) {
            history->label = QStringLiteral("Delete");
            history->icon = QStringLiteral("delete");
            history->style.background = warn;
            history->style.foreground = ThemeColors::contrastOn(warn);
            setCmd(history, QStringLiteral("compose.deleteName"));
        } else if (naming) {
            history->label.clear();
            history->icon.clear();
            history->style.background = QColor();
            history->style.foreground = QColor();
            setCmd(history, {});
        } else {
            history->label = QStringLiteral("History");
            history->icon = QStringLiteral("history");
            history->style.background = QColor();
            history->style.foreground = QColor();
            setCmd(history, QStringLiteral("compose.openHistory"));
        }
    }
}

bool ComposeUi::presentLive(const QString& id, PageDocument doc, QString* error)
{
    doc.id = id;
    return m_pages.attachDocument(std::move(doc), error, false);
}

bool ComposeUi::elevenMode() const
{
    return ElevenRequest::isElevenModel(ElevenRequest::normalizeModelId(m_settings.speechModel));
}

QString ComposeUi::currentVoiceId() const
{
    return elevenMode() ? m_settings.elevenVoiceId.trimmed() : m_settings.sapiVoiceToken.trimmed();
}

QString ComposeUi::currentVoiceDisplayName() const
{
    const QString id = currentVoiceId();
    for (const VoiceCatalog::Voice& v : currentVoices()) {
        if (v.id == id) {
            return v.name;
        }
    }
    return elevenMode() ? QStringLiteral("Voice") : QStringLiteral("SAPI");
}

void ComposeUi::syncClosedOverlays()
{
    if (nameEditing() && !hasLive(kItemEditLiveId)) {
        finishItemEditor(true);
    }
}

} // namespace gazer
