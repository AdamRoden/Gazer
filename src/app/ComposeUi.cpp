#include "app/ComposeUi.h"

#include "app/AppSettings.h"
#include "app/ComposeUiInternal.h"
#include "app/SettingsPageBuild.h"
#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"
#include "assist/SpeechEngine.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechHistory.h"
#include "assist/SpeechSecrets.h"
#include "assist/SystemVolume.h"
#include "assist/TtsService.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/BoardPaint.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFontMetrics>
#include <QObject>
#include <QStandardPaths>
#include <QStringView>
#include <QtGlobal>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace gazer {

using SettingsPageBuild::cell;
using compose_detail::commandAction;
using compose_detail::parseColor;

ComposeUi::ComposeUi(PageSession& pages, SpeechEngine& speech, AppSettings& settings,
                     SpeechSecrets& secrets, ElevenClient& eleven, TtsService& tts,
                     SoundboardStore& board, SpeechHistory& history)
    : m_pages(pages)
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
    loadPredictor();
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
    updatePredictions();
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
        return;
    }
    if (isOpen()) {
        refresh();
    }
}

void ComposeUi::insertText(QStringView chars)
{
    if (chars.isEmpty()) {
        return;
    }
    if (m_settings.composePredictions && !nameEditing() && chars == QLatin1String(" ")) {
        const WordPredictor::Query q = WordPredictor::fromPhrase(m_buffer.text(), m_buffer.caret());
        if (!q.typed.isEmpty() && m_predictor.isLexiconWord(q.typed)) {
            m_predictor.observe(q.sentenceWords, q.typed);
            if (!m_predictUserPath.isEmpty()) {
                m_predictor.saveUser(m_predictUserPath);
            }
        }
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
    stopListScroll();
    closeItemEdit();
    m_composeAuthored = {};
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
    m_speech.speak(text, SpeakKind::Composed, true);
    return true;
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

void ComposeUi::moveEndOfWord(int slot)
{
    m_buffer.moveCaretToVisibleWordEdge(slot, false);
    refresh();
}

void ComposeUi::moveStartOfWord(int slot)
{
    m_buffer.moveCaretToVisibleWordEdge(slot, true);
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
    if (!m_composeAuthored.isValid()) {
        m_composeAuthored = doc;
    }
    if (!doc.findCell(QStringLiteral("topic_0"))) {
        fillSoundboard(doc);
    }
    const auto st = m_speech.status();
    const bool stop = st.busy || st.speaking;
    const bool naming = nameEditing();
    const auto vis = naming ? QVector<ComposeBuffer::Token>{} : m_buffer.visibleTokens();

    auto paint = [&](PageGrid& g, auto& self) -> void {
        for (PageCell& c : g.cells) {
            if (c.id == QLatin1String("phrase")) {
                if (naming) {
                    const QString raw = m_buffer.text().trimmed();
                    c.label = ellipsis(raw, 24);
                    const int visLen = qMin(raw.size(), 24);
                    c.caretIndex = qBound(0, qBound(0, m_buffer.caret(), visLen), c.label.size());
                } else {
                    c.label = m_buffer.text();
                    c.caretIndex = qBound(0, m_buffer.caret(), c.label.size());
                    c.role = QStringLiteral("display");
                }
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
    layoutPredictRow(doc);
}

namespace {

int predictionWidth(const QString& label)
{
    const QFont font(BoardPaint::segoeFamily(), 16, QFont::DemiBold);
    const int adv = int(std::ceil(QFontMetricsF(font).horizontalAdvance(label)));
    return std::clamp(adv + 32, 72, 280);
}

QString displayWord(const QString& word, const QString& typed, bool capFirst)
{
    if (word == QLatin1String("i")) {
        return QStringLiteral("I");
    }
    bool anyLetter = false;
    bool allCaps = true;
    for (const QChar c : typed) {
        if (!c.isLetter()) {
            continue;
        }
        anyLetter = true;
        if (c.isLower()) {
            allCaps = false;
        }
    }
    if (anyLetter && allCaps && typed.size() > 1) {
        return word.toUpper();
    }
    if (capFirst && !word.isEmpty()) {
        QString s = word;
        s[0] = s[0].toUpper();
        return s;
    }
    return word;
}

} // namespace

void ComposeUi::loadPredictor()
{
    const QString path = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("resources/predict/model.bin"));
    QString err;
    if (!m_predictor.loadFile(path, &err) && !m_predictWarned) {
        m_predictWarned = true;
        GAZER_WARN << "Word predictions unavailable:" << err;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);
        m_predictUserPath = QDir(dir).filePath(QStringLiteral("predict-user.bin"));
        m_predictor.loadUser(m_predictUserPath);
    }
}

void ComposeUi::updatePredictions()
{
    m_shown.clear();
    if (nameEditing() || !m_settings.composePredictions || !m_predictor.isLoaded()) {
        return;
    }
    const QString text = m_buffer.text();
    const int caret = m_buffer.caret();
    const WordPredictor::Query q = WordPredictor::fromPhrase(text, caret);
    const QVector<WordPredictor::Hit> hits = m_predictor.suggest(q, WordPredictor::kMaxSuggestions);
    for (const WordPredictor::Hit& hit : hits) {
        ShownPrediction shown;
        int start = caret;
        int end = caret;
        bool trailing = true;
        bool cap = q.sentenceStart || hit.proper;
        QString raw;
        if (hit.replacesPrevious && q.previousStart >= 0) {
            start = q.previousStart;
            end = q.previousEnd;
            trailing = false;
            cap = q.sentenceWords.size() == 1 || hit.proper;
            raw = text.mid(start, end - start);
        } else if (!q.typed.isEmpty()) {
            start = q.activeStart;
            end = q.activeEnd;
            trailing = end >= text.size();
            cap = q.sentenceWords.isEmpty() || hit.proper;
            raw = text.mid(start, end - start);
        }
        shown.label = displayWord(hit.word, raw, cap);
        shown.start = start;
        shown.end = end;
        QString piece = shown.label;
        if (trailing) {
            piece.append(QLatin1Char(' '));
        }
        if (start > 0 && start <= text.size() && !text[start - 1].isSpace() && !piece.isEmpty()
            && !piece.front().isSpace()) {
            piece.prepend(QLatin1Char(' '));
        }
        shown.insertText = piece;
        shown.learnWord = hit.word;
        shown.learnLeft = q.sentenceWords;
        if (hit.replacesPrevious && !shown.learnLeft.isEmpty()) {
            shown.learnLeft.removeLast();
        }
        m_shown.push_back(std::move(shown));
    }
}

void ComposeUi::layoutPredictRow(PageDocument& doc) const
{
    PageGrid* predict = doc.findGrid(QStringLiteral("predict"));
    PageGrid* phrase = doc.findGrid(QStringLiteral("phrase"));
    const PageGrid* authPredict = m_composeAuthored.findGrid(QStringLiteral("predict"));
    const PageGrid* authPhrase = m_composeAuthored.findGrid(QStringLiteral("phrase"));
    const PageGrid* authChips = m_composeAuthored.findGrid(QStringLiteral("chips"));
    if (!predict || !phrase || !authPredict || !authPhrase) {
        return;
    }
    const int pRow = authPredict->row;
    const int phRow = authPhrase->row;
    const int phSpan = std::max(1, authPhrase->rowSpan);
    const bool hide = nameEditing() || !m_settings.composePredictions;
    if (hide) {
        predict->rowSpan = 0;
        predict->colSpan = 0;
        predict->cells.clear();
        predict->columnTracks.clear();
        phrase->row = std::min(pRow, phRow);
        int bottom = phRow + phSpan;
        if (nameEditing() && authChips) {
            bottom = std::max(bottom, authChips->row + std::max(1, authChips->rowSpan));
        }
        phrase->rowSpan = std::max(1, bottom - phrase->row);
        return;
    }
    predict->row = pRow;
    predict->rowSpan = std::max(1, authPredict->rowSpan);
    predict->col = authPredict->col;
    predict->colSpan = std::max(1, authPredict->colSpan);
    phrase->row = phRow;
    phrase->rowSpan = phSpan;
    predict->rows = 1;
    predict->cells.clear();
    predict->columnTracks.clear();
    if (m_shown.isEmpty()) {
        predict->columns = 1;
        PageCell empty;
        empty.id = QStringLiteral("pred_0");
        empty.row = 0;
        empty.col = 0;
        empty.role = QStringLiteral("label");
        predict->cells.push_back(std::move(empty));
        return;
    }
    predict->columns = m_shown.size();
    for (int i = 0; i < m_shown.size(); ++i) {
        PageCell cell;
        cell.id = QStringLiteral("pred_%1").arg(i);
        cell.label = m_shown[i].label;
        cell.row = 0;
        cell.col = i;
        cell.actions.push_back(
            commandAction(QStringLiteral("compose.acceptPrediction.%1").arg(i)));
        predict->cells.push_back(std::move(cell));
        predict->columnTracks.push_back(
            PageTrackSize::fromDim(PageDim::pixels(predictionWidth(m_shown[i].label))));
    }
}

void ComposeUi::acceptPrediction(int slot)
{
    if (nameEditing() || slot < 0 || slot >= m_shown.size()) {
        return;
    }
    const ShownPrediction shown = m_shown[slot];
    m_buffer.replaceRange(shown.start, shown.end, shown.insertText);
    if (m_settings.composePredictions) {
        m_predictor.observe(shown.learnLeft, shown.learnWord);
        if (!m_predictUserPath.isEmpty()) {
            m_predictor.saveUser(m_predictUserPath);
        }
    }
    refresh();
}

void ComposeUi::stampComposerChrome(PageDocument& doc) const
{
    if (!nameEditing()) {
        return;
    }

    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor ok(46, 125, 50);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);

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
        QString styleId;
        int keyCol = 0;
        int keyColSpan = 1;
        if (!keys->cells.isEmpty()) {
            styleId = keys->cells.first().styleId;
            keyCol = keys->cells.first().col;
            keyColSpan = qMax(1, keys->cells.first().colSpan);
        }
        keys->cells.clear();
        PageCell color = cell(QStringLiteral("clear"), QStringLiteral("Color"), 0, keyCol,
                              QStringLiteral("compose.editShowColors"),
                              !m_editIcons ? accent : surface, keyColSpan, {}, {},
                              QStringLiteral("palette"));
        color.styleId = styleId;
        color.activeState = QStringLiteral("compose.editColors");
        keys->cells.push_back(std::move(color));
        PageCell image = cell(QStringLiteral("undo"), QStringLiteral("Icon"), 1, keyCol,
                              QStringLiteral("compose.editShowIcons"),
                              m_editIcons ? accent : surface, keyColSpan, {}, {},
                              QStringLiteral("photoCamera"));
        image.styleId = styleId;
        image.activeState = QStringLiteral("compose.editIcons");
        keys->cells.push_back(std::move(image));
        keys->rows = keys->cells.size();
    }

    if (PageGrid* phraseGrid = doc.findGrid(QStringLiteral("phrase"))) {
        if (const PageGrid* chips = doc.findGrid(QStringLiteral("chips"))) {
            const int chipBottom = chips->row + qMax(0, chips->rowSpan);
            phraseGrid->rowSpan = qMax(phraseGrid->rowSpan, chipBottom - phraseGrid->row);
        }
    }
    if (PageGrid* chips = doc.findGrid(QStringLiteral("chips"))) {
        chips->rowSpan = 0;
        chips->colSpan = 0;
        chips->cells.clear();
    }

    if (PageCell* phrase = doc.findCell(QStringLiteral("phrase"))) {
        const QColor bg = parseColor(m_editColor, surface);
        phrase->label = ellipsis(m_buffer.text().trimmed(), 24);
        phrase->caretIndex = qBound(0, m_buffer.caret(), phrase->label.size());
        phrase->icon = m_editIcon;
        phrase->caption.clear();
        phrase->role = QStringLiteral("display");
        phrase->textStyle = QStringLiteral("title");
        phrase->style.background = bg;
        phrase->style.foreground = ThemeColors::contrastOn(bg);
        phrase->style.borderColor = accent;
        phrase->style.thickness = PageBox::all(3);
        phrase->actions.clear();
    }

    if (PageCell* speak = doc.findCell(QStringLiteral("speak"))) {
        speak->label = QStringLiteral("Save");
        speak->icon = QStringLiteral("check");
        speak->style.background = ok;
        speak->style.foreground = QColor(255, 255, 255);
        speak->activeState.clear();
        setCmd(speak, QStringLiteral("compose.saveName"));
    }
    if (PageCell* voices = doc.findCell(QStringLiteral("voices"))) {
        voices->label = QStringLiteral("Cancel");
        voices->icon = QStringLiteral("close");
        voices->style.background = surface;
        setCmd(voices, QStringLiteral("compose.cancelName"));
    }
    if (PageCell* history = doc.findCell(QStringLiteral("history"))) {
        if (nameEditCanDelete()) {
            history->label = QStringLiteral("Delete");
            history->icon = QStringLiteral("delete");
            history->style.background = warn;
            history->style.foreground = ThemeColors::contrastOn(warn);
            setCmd(history, QStringLiteral("compose.deleteName"));
        } else {
            history->label.clear();
            history->icon.clear();
            history->style.background = QColor();
            history->style.foreground = QColor();
            setCmd(history, {});
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
    if (m_listScroll == ListScroll::History && !hasLive(kHistoryLiveId)) {
        stopListScroll();
    } else if (m_listScroll == ListScroll::Voices && !hasLive(kVoicesLiveId)) {
        stopListScroll();
    }
    if (nameEditing() && !hasLive(kItemEditLiveId)) {
        finishItemEditor(true);
    }
}

} // namespace gazer
