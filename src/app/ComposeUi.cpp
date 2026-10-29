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
#include "assist/TtsService.h"
#include "assist/VoiceStyles.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "predict/TypingContext.h"
#include "ui/BoardPaint.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QObject>
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
{
    loadPredictor();
}

ComposeUi::~ComposeUi() = default;

void ComposeUi::setTypingContext(TypingContext* typing)
{
    m_typing = typing;
    if (m_typing && !capturingPhrase()) {
        m_typing->syncPhrase(m_buffer.text(), m_buffer.caret());
    }
}

bool ComposeUi::isCapturing(const QString& sourcePageId) const
{
    return sourcePageId == kPageId || sourcePageId == kVoicesLiveId
           || sourcePageId == kHistoryLiveId || sourcePageId == kFilesLiveId
           || sourcePageId == kItemEditLiveId;
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

bool ComposeUi::nameEditing() const
{
    return m_interaction == Interaction::NameEdit;
}

bool ComposeUi::capturingPhrase() const
{
    return nameEditing() || m_styleNaming;
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
           || name == QLatin1String("volume.dec")
           || name == QLatin1String("volume.inc")
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
    rebuildFiles();
    if (m_typing && !capturingPhrase() && !editingFileName()) {
        m_typing->syncPhrase(m_buffer.text(), m_buffer.caret());
    }
}

void ComposeUi::refreshLiveBoards()
{
    rebuildVoices();
    rebuildHistory();
    rebuildItemEdit();
    refresh();
}

void ComposeUi::syncFromSettings()
{
    if (isOpen()) {
        refresh();
    }
}

void ComposeUi::insertText(QStringView chars)
{
    if (chars.isEmpty()) {
        return;
    }
    if (m_settings.composePredictions && !capturingPhrase() && chars == QLatin1String(" ")) {
        const WordPredictor::Query q = WordPredictor::fromPhrase(m_buffer.text(), m_buffer.caret());
        if (!q.typed.isEmpty() && m_predictor.isLexiconWord(q.typed)) {
            if (m_typing) {
                m_typing->noteAcceptedWord(q.sentenceWords, q.typed);
            } else {
                m_predictor.observe(q.sentenceWords, q.typed);
            }
        }
    }
    QStringView text = chars;
    QString capped;
    if (m_styleNaming) {
        const int room = VoiceStyles::kNameMax - m_buffer.text().size();
        if (room <= 0) {
            return;
        }
        if (text.size() > room) {
            capped = text.left(room).toString();
            text = capped;
        }
    }
    m_buffer.insert(text);
    commitEdit();
}

void ComposeUi::handleSend(const PageAction& a)
{
    if (handleFileSend(a)) {
        return;
    }
    const QString top = m_pages.topPageId();
    if (top != kPageId && !capturingPhrase()) {
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
        if (m_styleNaming) {
            cancelStyleName();
            return;
        }
        if (nameEditing()) {
            cancelNameEdit();
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
        if (handleFileEscape()) {
            return;
        }
        m_pages.closePage(QString(kPageId));
        return;
    }
    if (handleFileCommand(c)) {
        return;
    }
    if (capturingPhrase()) {
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
            if (m_styleNaming) {
                saveStyleName(&err);
            } else {
                saveNameEdit(&err);
            }
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
    if (m_styleNaming) {
        m_styleNaming = false;
        m_buffer.load(m_nameEditBackup);
        if (m_typing) {
            m_typing->adoptPhrase(m_buffer.text(), m_buffer.caret());
        }
        m_nameEdit = NameEditKind::None;
        m_editId.clear();
        m_nameEditBackup.clear();
    } else if (nameEditing()) {
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
    m_styleEditor = false;
    m_stylePage = 0;
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
    if (m_styleNaming) {
        return saveStyleName(error);
    }
    if (nameEditing()) {
        return saveNameEdit(error);
    }
    if (consumeFileSpeak(error)) {
        return true;
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
    editTarget().clear();
    commitEdit();
}

bool ComposeUi::undo()
{
    const bool ok = editTarget().undo();
    commitEdit();
    return ok;
}

bool ComposeUi::redo()
{
    const bool ok = editTarget().redo();
    commitEdit();
    return ok;
}

void ComposeUi::backspace()
{
    editTarget().backspace();
    commitEdit();
}

void ComposeUi::deleteWord()
{
    editTarget().deleteWord();
    commitEdit();
}

void ComposeUi::removeVisibleWord(int slot)
{
    m_buffer.removeVisibleWord(slot);
    commitEdit();
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

void ComposeUi::selectAll()
{
    if (capturingPhrase()) {
        return;
    }
    editTarget().selectAll();
    refresh();
}

void ComposeUi::copy()
{
    if (capturingPhrase()) {
        return;
    }
    QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    const ComposeBuffer& buf = editTarget();
    clip->setText(buf.hasSelection() ? buf.selectedText() : buf.text());
}

void ComposeUi::cut()
{
    if (capturingPhrase()) {
        return;
    }
    ComposeBuffer& buf = editTarget();
    if (!buf.hasSelection()) {
        return;
    }
    if (QClipboard* clip = QGuiApplication::clipboard()) {
        clip->setText(buf.selectedText());
    }
    buf.backspace();
    commitEdit();
}

void ComposeUi::paste()
{
    if (capturingPhrase()) {
        return;
    }
    QClipboard* clip = QGuiApplication::clipboard();
    if (!clip) {
        return;
    }
    const QString text = clip->text();
    if (text.isEmpty()) {
        return;
    }
    if (editingFileName()) {
        insertFileName(text);
        return;
    }
    m_buffer.insert(text);
    commitEdit();
}

void ComposeUi::pageChips(int delta)
{
    if (capturingPhrase()) {
        return;
    }
    m_buffer.pageChips(delta);
    refresh();
}

void ComposeUi::setSpeechModel(const QString& model)
{
    m_settings.speechModel = ElevenRequest::normalizeModelId(model);
    m_voicePage = 0;
    apply();
}

void ComposeUi::nudgeSpeed(int dir)
{
    if (!m_settings.nudge(QStringLiteral("speechSpeed"), dir)) {
        return;
    }
    apply();
}

void ComposeUi::nudgeVolume(int dir)
{
    if (!m_settings.nudge(QStringLiteral("speechVolume"), dir)) {
        return;
    }
    apply();
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
    const bool naming = capturingPhrase();
    if (!naming) {
        stampChips(doc);
    }
    stampStyleLayer(doc);

    auto paint = [&](PageGrid& g, auto& self) -> void {
        for (PageCell& c : g.cells) {
            if (c.id == QLatin1String("phrase")) {
                c.selectionStart = -1;
                c.selectionEnd = -1;
                if (naming) {
                    presentNamingPhrase(c);
                } else {
                    c.label = m_buffer.text();
                    c.caretIndex = qBound(0, m_buffer.caret(), c.label.size());
                    if (m_buffer.hasSelection()) {
                        c.selectionStart = m_buffer.selectionStart();
                        c.selectionEnd = m_buffer.selectionEnd();
                    }
                    c.role = QStringLiteral("display");
                    if (editMode()) {
                        c.caption = QStringLiteral("Dwell a button or + topic");
                    } else if (assignMode()) {
                        c.caption = QStringLiteral("Dwell a cell to save");
                    } else if (!m_boardNote.isEmpty()) {
                        c.caption = m_boardNote;
                    } else if (m_speech.elevenLatched()) {
                        c.caption = QStringLiteral("Using SAPI (network)");
                    } else {
                        c.caption.clear();
                    }
                }
            } else if (c.id == QLatin1String("speak") && !naming) {
                c.label = stop ? QStringLiteral("Stop") : QStringLiteral("Speak");
                c.icon = stop ? QStringLiteral("Sleep") : QStringLiteral("Speak");
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
}

void ComposeUi::updatePredictions()
{
    m_shown.clear();
    if (capturingPhrase() || !m_settings.composePredictions || !m_predictor.isLoaded()) {
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
    const bool hide = capturingPhrase() || !m_settings.composePredictions;
    if (hide) {
        predict->rowSpan = 0;
        predict->colSpan = 0;
        predict->cells.clear();
        predict->columnTracks.clear();
        phrase->row = std::min(pRow, phRow);
        int bottom = phRow + phSpan;
        if (capturingPhrase() && authChips) {
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
    if (capturingPhrase() || slot < 0 || slot >= m_shown.size()) {
        return;
    }
    const ShownPrediction shown = m_shown[slot];
    m_buffer.replaceRange(shown.start, shown.end, shown.insertText);
    if (m_settings.composePredictions) {
        if (m_typing) {
            m_typing->noteAcceptedWord(shown.learnLeft, shown.learnWord);
        } else {
            m_predictor.observe(shown.learnLeft, shown.learnWord);
        }
    }
    refresh();
}

void ComposeUi::stampChips(PageDocument& doc) const
{
    PageGrid* chips = doc.findGrid(QStringLiteral("chips"));
    const PageGrid* authored = m_composeAuthored.findGrid(QStringLiteral("chips"));
    if (!chips || !authored) {
        return;
    }
    chips->row = authored->row;
    chips->col = authored->col;
    chips->rowSpan = std::max(1, authored->rowSpan);
    chips->colSpan = std::max(1, authored->colSpan);
    chips->cells = authored->cells;
    const QVector<ComposeBuffer::Token> tokens = m_buffer.tokens();
    for (PageCell& cell : chips->cells) {
        if (cell.id == QLatin1String("chip_more")) {
            cell.label.clear();
            continue;
        }
        if (!cell.id.startsWith(QLatin1String("chip_"))) {
            continue;
        }
        bool ok = false;
        const int slot = QStringView(cell.id).mid(5).toInt(&ok);
        if (!ok) {
            continue;
        }
        const ComposeBuffer::ChipSlot chip = m_buffer.chipSlot(slot);
        if (chip.kind == ComposeBuffer::ChipSlotKind::Word) {
            cell.label = chip.tokenIndex >= 0 && chip.tokenIndex < tokens.size()
                             ? tokens.at(chip.tokenIndex).text
                             : QString();
            continue;
        }
        cell.phases.clear();
        cell.actions.clear();
        if (chip.kind == ComposeBuffer::ChipSlotKind::PageBack
            || chip.kind == ComposeBuffer::ChipSlotKind::PageForward) {
            const bool back = chip.kind == ComposeBuffer::ChipSlotKind::PageBack;
            cell.label = back ? QString(u"\u25C05") : QString(u"5\u25B6");
            cell.role.clear();
            cell.actions.push_back(commandAction(
                QStringLiteral("compose.pageChips.%1")
                    .arg(back ? -ComposeBuffer::kChipPageStep : ComposeBuffer::kChipPageStep)));
        } else {
            cell.label.clear();
            cell.role = QStringLiteral("label");
        }
    }
}

void ComposeUi::presentNamingPhrase(PageCell& cell) const
{
    const int cap = m_styleNaming ? VoiceStyles::kNameMax : 24;
    const QString raw = m_buffer.text().trimmed();
    cell.label = ellipsis(raw, cap);
    const int visLen = qMin(raw.size(), cap);
    cell.caretIndex = qBound(0, qBound(0, m_buffer.caret(), visLen), cell.label.size());
    cell.selectionStart = -1;
    cell.selectionEnd = -1;
    cell.caption = m_styleNaming ? QStringLiteral("Type a style name, then Save")
                                 : QStringLiteral("Type a name, then Save");
    cell.textStyle = QStringLiteral("title");
    cell.role = QStringLiteral("display");
}

void ComposeUi::stampComposerChrome(PageDocument& doc) const
{
    if (!capturingPhrase()) {
        return;
    }

    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor ok(46, 125, 50);
    const QColor warn = theme.danger.isValid() ? theme.danger : QColor(180, 80, 80);
    const QColor surface = theme.defaultCell();
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);

    if (PageGrid* keys = doc.findGrid(QStringLiteral("edit_keys"))) {
        QString styleId;
        int keyCol = 0;
        int keyColSpan = 1;
        if (!keys->cells.isEmpty()) {
            styleId = keys->cells.first().styleId;
            keyCol = keys->cells.first().col;
            keyColSpan = qMax(1, keys->cells.first().colSpan);
        }
        keys->row = 0;
        keys->rowSpan = 3;
        keys->cells.clear();
        if (m_styleNaming) {
            PageCell clear = cell(QStringLiteral("clear"), QStringLiteral("Clear"), 0, keyCol,
                                  QStringLiteral("compose.clear"), surface, keyColSpan);
            clear.styleId = styleId;
            keys->cells.push_back(std::move(clear));
        } else {
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
        }
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
        presentNamingPhrase(*phrase);
        const QColor bg = parseColor(m_editColor, surface);
        phrase->icon = m_editIcon;
        phrase->style.background = bg;
        phrase->style.foreground = ThemeColors::contrastOn(bg);
        phrase->style.borderColor = accent;
        phrase->style.thickness = PageBox::all(3);
        phrase->actions.clear();
    }

    if (PageGrid* side = doc.findGrid(QStringLiteral("speak_keys"))) {
        QString styleId;
        int keyCol = 0;
        int keyColSpan = 1;
        if (!side->cells.isEmpty()) {
            styleId = side->cells.first().styleId;
            keyCol = side->cells.first().col;
            keyColSpan = qMax(1, side->cells.first().colSpan);
        }
        side->row = 0;
        side->rowSpan = 3;
        side->cells.clear();
        PageCell save = cell(QStringLiteral("speak"), QStringLiteral("Save"), 0, keyCol,
                             QStringLiteral("compose.saveName"), ok, keyColSpan, {}, {},
                             QStringLiteral("check"));
        save.styleId = styleId;
        save.style.foreground = QColor(255, 255, 255);
        save.activeState.clear();
        side->cells.push_back(std::move(save));
        PageCell cancel = cell(QStringLiteral("voices"), QStringLiteral("Cancel"), 1, keyCol,
                               QStringLiteral("compose.cancelName"), surface, keyColSpan, {}, {},
                               QStringLiteral("close"));
        cancel.styleId = styleId;
        side->cells.push_back(std::move(cancel));
        if (nameEditCanDelete()) {
            PageCell del = cell(QStringLiteral("history"), QStringLiteral("Delete"), 2, keyCol,
                                QStringLiteral("compose.deleteName"), warn, keyColSpan, {}, {},
                                QStringLiteral("delete"));
            del.styleId = styleId;
            del.style.foreground = ThemeColors::contrastOn(warn);
            side->cells.push_back(std::move(del));
        }
        side->rows = side->cells.size();
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

void ComposeUi::syncClosedOverlays()
{
    if (m_listScroll == ListScroll::History && !hasLive(kHistoryLiveId)) {
        stopListScroll();
    } else if (m_listScroll == ListScroll::Voices && !hasLive(kVoicesLiveId)) {
        stopListScroll();
    } else if (m_listScroll == ListScroll::Files && !hasLive(kFilesLiveId)) {
        stopListScroll();
    }
    if (nameEditing() && !hasLive(kItemEditLiveId)) {
        finishItemEditor(true);
    }
}

} // namespace gazer
