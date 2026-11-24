#include "app/ComposeUi.h"

#include "input/KeyStateManager.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "predict/TypingContext.h"
#include "ui/BoardPaint.h"
#include "ui/KeySymbols.h"
#include "ui/OverlaySurface.h"
#include "ui/PageHostWindow.h"
#include "ui/PhraseLayout.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QTimer>

namespace gazer {
namespace {

const PageTarget* phraseTarget(PageSession& pages)
{
    const QVector<PageTarget>& targets = pages.targets();
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets.at(i);
        if (t.pageId == ComposeUi::kPageId && localIdOf(t) == QLatin1String("phrase")) {
            return &t;
        }
    }
    return nullptr;
}

const PageTarget* fileNameTarget(PageSession& pages)
{
    const QVector<PageTarget>& targets = pages.targets();
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets.at(i);
        if (t.pageId == ComposeUi::kFilesLiveId && localIdOf(t) == QLatin1String("file_name")) {
            return &t;
        }
    }
    return nullptr;
}

QRectF screenAround(const QPointF& p)
{
    QScreen* screen = QGuiApplication::screenAt(p.toPoint());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen ? QRectF(screen->geometry()) : QRectF(0, 0, 1920, 1080);
}

QString frontPageAt(PageSession& pages, const QRectF& cell)
{
    if (cell.isEmpty()) {
        return {};
    }
    const QTransform* xf = pages.window() ? &pages.window()->drawerXf() : nullptr;
    return PageHit::coveringPageId(pages.gridPaints(), cell.center(), pages.drawerScale(),
                                   pages.targets(), xf);
}

QVector<PhraseCaretLine> caretLines(const QVector<PhraseLineGeom>& lines)
{
    QVector<PhraseCaretLine> out;
    out.reserve(lines.size());
    for (const PhraseLineGeom& line : lines) {
        PhraseCaretLine g;
        g.start = line.start;
        g.end = line.end;
        g.rect = line.rect;
        g.cursorX = line.cursorX;
        out.push_back(std::move(g));
    }
    return out;
}

void paintChoiceRing(QPainter& p, const QPointF& c, double radius, double progress, const QColor& ink)
{
    const QRectF arc(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 90), 2.0));
    p.drawEllipse(arc);
    if (progress <= 0.001) {
        return;
    }
    p.setPen(QPen(ink, 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arc, 90 * 16, int(-progress * 360.0 * 16.0));
}

const QColor kPhraseCopyAckColor(0x34, 0xA8, 0x53);
const QColor kPhraseButtonDisc(28, 32, 40);

[[nodiscard]] bool phraseCopyAckOn(const QElapsedTimer& timer)
{
    return timer.isValid() && static_cast<double>(timer.elapsed()) < kPhraseCopyAckMs;
}

QColor phraseCopyBorder(const PageHostWindow* window)
{
    if (window != nullptr && window->theme().accent.isValid()) {
        return window->theme().accent;
    }
    return QColor(0x8a, 0xb4, 0xf8);
}

int selectablePhraseWord(const QVector<PhraseWordSpan>& words, const QPointF& gaze, int bold,
                         int selStart, int selEnd)
{
    const int word = phraseWordIndexAt(words, gaze, bold);
    if (word < 0) {
        return -1;
    }
    const PhraseWordSpan& span = words.at(word);
    if (phraseWordInsideSelection(span.start, span.end, selStart, selEnd)) {
        return -1;
    }
    return word;
}

} // namespace

class PhraseCaretOverlay final : public OverlaySurface {
public:
    struct View {
        bool shown = false;
        QPointF center;
        bool hot = false;
        double progress = 0.0;
        QColor border;
        QColor ink;
        bool copyAck = false;
    };

    void setView(const View& view)
    {
        m_view = view;
        if (!view.shown) {
            hide();
            return;
        }
        const double r = kPhraseCaretButtonRadius;
        QRectF box(view.center.x() - r, view.center.y() - r, r * 2.0, r * 2.0);
        box.adjust(-18.0, -18.0, 18.0, 18.0);
        setGeometry(box.toAlignedRect());
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_view.shown) {
            return;
        }
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c = m_view.center - QPointF(pos());
        const double r = kPhraseCaretButtonRadius;
        const QColor border = m_view.border.isValid() ? m_view.border : QColor(0x8a, 0xb4, 0xf8);
        const QColor ink = m_view.ink.isValid() ? m_view.ink : QColor(255, 255, 255);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 80));
        p.drawEllipse(c + QPointF(0, 2), r, r);
        p.setPen(QPen(border, kPhraseCopyBorderPx));
        p.setBrush(m_view.hot ? QColor(48, 54, 64, 242)
                              : QColor(kPhraseButtonDisc.red(), kPhraseButtonDisc.green(),
                                       kPhraseButtonDisc.blue(), 235));
        p.drawEllipse(c, r, r);
        if (m_view.hot && !m_view.copyAck) {
            paintChoiceRing(p, c, r + 8.0, m_view.progress, ink);
        }
        const double side = r * 1.35;
        const QRectF icon(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
        KeySymbols::paint(p, QLatin1String(m_view.copyAck ? "check" : "contentCopy"), icon,
                          m_view.copyAck ? kPhraseCopyAckColor : QColor(255, 255, 255, 235));
    }

private:
    View m_view;
};

void ComposeUi::destroyPhraseChooser()
{
    delete m_caretUi;
    m_caretUi = nullptr;
    destroyStyleChip();
}

void ComposeUi::setPhraseKeys(KeyStateManager* keys)
{
    m_keys = keys;
}

bool ComposeUi::exportCaretEngaged() const
{
    return hasLive(kFilesLiveId) && m_fileMode == FileMode::Export && m_fileConfirm == FileConfirm::None
           && m_pages.topPageId() == kFilesLiveId;
}

void ComposeUi::useCaretHost(CaretLook::Host host)
{
    if (m_caret.host == host) {
        return;
    }
    if (m_caret.host == CaretLook::Host::Phrase) {
        publishCaretGrow(phraseTarget(m_pages), -1, -1, 1.0);
    } else if (m_caret.host == CaretLook::Host::File) {
        publishCaretGrow(fileNameTarget(m_pages), -1, -1, 1.0);
    }
    m_caret = {};
    m_caret.host = host;
    if (m_caretUi != nullptr) {
        m_caretUi->hide();
    }
}

ComposeBuffer& ComposeUi::caretBuffer()
{
    return m_caret.host == CaretLook::Host::File ? m_fileName : m_buffer;
}

const ComposeBuffer& ComposeUi::caretBuffer() const
{
    return m_caret.host == CaretLook::Host::File ? m_fileName : m_buffer;
}

const PageTarget* ComposeUi::caretTarget()
{
    return m_caret.host == CaretLook::Host::File ? fileNameTarget(m_pages) : phraseTarget(m_pages);
}

int ComposeUi::caretBoldWord() const
{
    if (m_caret.wordLook.latched || m_caret.wordLook.scanning || m_caret.wordLook.elapsedMs <= 0.0) {
        return -1;
    }
    return m_caret.wordLook.id;
}

bool ComposeUi::caretShift() const
{
    const bool held = m_keys != nullptr && m_keys->isHeld(QStringLiteral("shift"));
    return phraseShiftSelects(held, m_pages.attachedCopy(QString(kPageId)).showLayers);
}

int ComposeUi::phraseCaretLine(const QString& label, int caret) const
{
    const PageTarget* phrase = phraseTarget(m_pages);
    if (phrase == nullptr || caret < 0) {
        return -1;
    }
    const QRectF cell = phrase->geom.contentOnScreen();
    if (cell.isEmpty()) {
        return -1;
    }
    const PhraseTextFrame frame =
        phraseTextFrame(BoardPaint::segoeFamily(), cell, label, phrase->caption, phrase->textStyle,
                        phrase->role, true, false);
    if (!frame.valid) {
        return -1;
    }
    const QVector<PhraseLineGeom> lines =
        layoutPhraseLines(frame.textRect, label, frame.font, frame.flags);
    return phraseBoundaryCaretLine(caretLines(lines), caret);
}

void ComposeUi::syncCaretTyping()
{
    if (m_caret.host != CaretLook::Host::Phrase || m_typing == nullptr || capturingPhrase()
        || editingFileName()) {
        return;
    }
    m_typing->syncPhrase(m_buffer.text(), m_buffer.caret());
}

void ComposeUi::hideCaretCopy()
{
    m_caret.shown = false;
    m_caret.buttonLook = {};
    if (m_caretUi != nullptr) {
        m_caretUi->hide();
    }
}

void ComposeUi::dismissPhraseChooser()
{
    m_caret.wordLook = {};
}

void ComposeUi::showCaretCopy(const QRectF& anchor)
{
    m_caret.shown = true;
    if (m_caretUi == nullptr) {
        m_caretUi = new PhraseCaretOverlay();
    }
    layoutPhraseCopyButton(anchor, screenAround(anchor.center()), m_caret.button);
}

void ComposeUi::idleCaret(const PageTarget* target)
{
    m_caret.words.clear();
    m_caret.wordLook = {};
    m_caret.gazeLive = false;
    publishCaretGrow(target, -1, -1, 1.0);
    hideCaretCopy();
}

void ComposeUi::updateCaretChooser(const QPointF& gaze, bool gazeValid)
{
    if (!m_caret.shown) {
        if (m_caretUi != nullptr) {
            m_caretUi->hide();
        }
        return;
    }
    if (m_caretUi == nullptr) {
        m_caretUi = new PhraseCaretOverlay();
    }
    const ComposeBuffer& buffer = caretBuffer();
    bool onWord = false;
    if (gazeValid) {
        onWord = selectablePhraseWord(m_caret.words, gaze, caretBoldWord(), buffer.selectionStart(),
                                      buffer.selectionEnd())
                 >= 0;
    }
    const bool copyAck = phraseCopyAckOn(m_caret.ack);
    const bool hot = gazeValid && !onWord && !copyAck && phraseCaretButtonAt(m_caret.button, gaze);
    PhraseCaretOverlay::View view;
    view.shown = true;
    view.center = m_caret.button.center;
    view.hot = hot;
    view.copyAck = copyAck;
    view.ink = QColor(m_settings.progressColor);
    view.border = phraseCopyBorder(m_pages.window());
    if (hot && !m_caret.buttonLook.latched) {
        view.progress = m_caret.buttonLook.elapsedMs / kPhraseWordDwellMs;
    }
    m_caretUi->setView(view);
}

void ComposeUi::publishCaretGrow(const PageTarget* target, int start, int end, double scale)
{
    PageHostWindow* window = m_pages.window();
    if (window == nullptr || target == nullptr) {
        return;
    }
    window->setPhraseWordGrow(sessionKey(*target), start, end, scale);
}

double ComposeUi::caretLookDt(const GazePoint& point)
{
    qint64 now = point.timestampMs;
    if (now <= 0) {
        if (!m_caretClock.isValid()) {
            m_caretClock.start();
        }
        now = m_caretClock.elapsed();
    }
    double dt = 0.0;
    if (m_caretLookMs >= 0 && now > m_caretLookMs) {
        dt = qBound(0.0, double(now - m_caretLookMs), 80.0);
    }
    m_caretLookMs = now;
    return dt;
}

void ComposeUi::publishCaret(int caretBefore, int selStartBefore, int selEndBefore)
{
    ComposeBuffer& buffer = caretBuffer();
    const bool predictions = m_caret.host == CaretLook::Host::Phrase;
    const bool moved = buffer.caret() != caretBefore || buffer.selectionStart() != selStartBefore
                       || buffer.selectionEnd() != selEndBefore;
    if (moved && predictions && !capturingPhrase() && m_settings.composePredictions) {
        const QVector<ShownPrediction> before = m_shown;
        updatePredictions();
        if (before != m_shown) {
            refresh();
        }
    }
    PageHostWindow* window = m_pages.window();
    const PageTarget* target = caretTarget();
    if (window == nullptr || target == nullptr) {
        return;
    }
    int cap = target->label.size();
    if (target->label != buffer.text() && target->label.endsWith(QChar(0x2026))) {
        cap = qMax(0, cap - 1);
    }
    const int caret = qBound(0, buffer.caret(), cap);
    int selA = buffer.selectionStart();
    int selB = buffer.selectionEnd();
    if (selA >= 0 && selB >= 0) {
        selA = qBound(0, selA, cap);
        selB = qBound(0, selB, cap);
        if (selB <= selA) {
            selA = -1;
            selB = -1;
        }
    }
    int line = -1;
    if (m_caret.host != CaretLook::Host::File) {
        line = phraseCaretLine(target->label, caret);
    }
    window->setTargetCaret(sessionKey(*target), caret, line, selA, selB);
}

bool ComposeUi::claimsCaretGaze(const GazePoint& point) const
{
    if (m_pages.isDwellSuspended() || !point.valid) {
        return false;
    }
    const QPointF g = point.toPointF();
    if (m_styleChipShown && phraseStyleChipAt(m_styleChip, g)) {
        return true;
    }
    if (!m_caret.gazeLive) {
        return false;
    }
    const ComposeBuffer& buffer = caretBuffer();
    if (selectablePhraseWord(m_caret.words, g, caretBoldWord(), buffer.selectionStart(),
                             buffer.selectionEnd())
        >= 0) {
        return true;
    }
    return m_caret.shown && phraseCaretButtonAt(m_caret.button, g);
}

void ComposeUi::applyCaretChoice(PhraseCaretChoice choice)
{
    if (m_caret.host == CaretLook::Host::File
        && (!hasLive(kFilesLiveId) || m_fileMode != FileMode::Export)) {
        return;
    }
    ComposeBuffer& buffer = caretBuffer();
    const int before = buffer.caret();
    const int selBefore = buffer.selectionStart();
    const int selEndBefore = buffer.selectionEnd();
    const PhraseCaretEdit edit =
        applyPhraseCaretChoice(buffer.caret(), buffer.selectionStart(), buffer.selectionEnd(), choice,
                               caretShift(), buffer.text().size());
    if (!edit.changed) {
        return;
    }
    if (edit.selStart < 0) {
        buffer.setCaret(edit.caret);
    } else {
        buffer.setSelectionRange(edit.selStart, edit.selEnd, edit.caret);
    }
    syncCaretTyping();
    publishCaret(before, selBefore, selEndBefore);
    if (!buffer.hasSelection()) {
        m_caret.copiedStart = -1;
        m_caret.copiedEnd = -1;
    }
    const bool blockCopy = m_caret.host == CaretLook::Host::Phrase && capturingPhrase();
    if (!phraseCopyAckOn(m_caret.ack) && (!buffer.hasSelection() || blockCopy)) {
        hideCaretCopy();
    }
}

void ComposeUi::selectCaretWord(int index)
{
    if (index < 0 || index >= m_caret.words.size()) {
        return;
    }
    ComposeBuffer& buffer = caretBuffer();
    const PhraseWordSpan& word = m_caret.words.at(index);
    const int before = buffer.caret();
    const int selBefore = buffer.selectionStart();
    const int selEndBefore = buffer.selectionEnd();
    const PhraseCaretEdit edit =
        selectPhraseWordRange(before, selBefore, selEndBefore, word.start, word.end, caretShift());
    if (!edit.changed) {
        return;
    }
    buffer.setSelectionRange(edit.selStart, edit.selEnd, edit.caret);
    syncCaretTyping();
    publishCaret(before, selBefore, selEndBefore);
}

void ComposeUi::nudgeActiveCaret(int delta)
{
    if (delta == 0) {
        return;
    }
    applyCaretChoice(delta < 0 ? PhraseCaretChoice::Left : PhraseCaretChoice::Right);
    finishCaretKey();
}

void ComposeUi::nudgePhraseCaret(int delta)
{
    if (delta == 0) {
        return;
    }
    useCaretHost(CaretLook::Host::Phrase);
    nudgeActiveCaret(delta);
}

void ComposeUi::nudgeFileCaret(int delta)
{
    if (delta == 0 || !hasLive(kFilesLiveId) || m_fileMode != FileMode::Export) {
        return;
    }
    useCaretHost(CaretLook::Host::File);
    if (delta == -1 || delta == 1) {
        nudgeActiveCaret(delta);
        return;
    }
    ComposeBuffer& buffer = caretBuffer();
    const int before = buffer.caret();
    const int selBefore = buffer.selectionStart();
    const int selEndBefore = buffer.selectionEnd();
    const int next = qBound(0, buffer.caret() + delta, buffer.text().size());
    if (next == before && !buffer.hasSelection()) {
        return;
    }
    buffer.setCaret(next);
    m_caret.copiedStart = -1;
    m_caret.copiedEnd = -1;
    publishCaret(before, selBefore, selEndBefore);
    hideCaretCopy();
    publishCaretGrow(caretTarget(), -1, -1, 1.0);
}

void ComposeUi::finishCaretKey()
{
    publishCaretGrow(caretTarget(), -1, -1, 1.0);
    updateCaretChooser({}, false);
}

bool ComposeUi::handlePhraseCaretKey(const QString& command)
{
    const bool caretKey = command == QLatin1String("arrowLeft") || command == QLatin1String("arrowRight")
                          || command == QLatin1String("home") || command == QLatin1String("end");
    if (textFieldOpen()) {
        return caretKey && nudgeFieldCaret(command);
    }
    if (caretKey && nameEditing() && !borrowingPhrase()) {
        return true;
    }
    if (command == QLatin1String("arrowLeft") || command == QLatin1String("arrowRight")) {
        nudgePhraseCaret(command == QLatin1String("arrowLeft") ? -1 : 1);
        return true;
    }
    if (command == QLatin1String("home") || command == QLatin1String("end")) {
        useCaretHost(CaretLook::Host::Phrase);
        applyCaretChoice(command == QLatin1String("home") ? PhraseCaretChoice::Home
                                                          : PhraseCaretChoice::End);
        finishCaretKey();
        return true;
    }
    return false;
}

void ComposeUi::runCaret(const GazePoint& point, const QRectF& anchor)
{
    m_caret.gazeLive = true;
    ComposeBuffer& buffer = caretBuffer();
    const QPointF gaze = point.toPointF();
    const bool gazeValid = point.valid;
    const double dt = gazeValid ? caretLookDt(point) : 0.0;
    if (!gazeValid) {
        m_caret.wordLook = {};
        m_caret.buttonLook = {};
    }

    int growStart = -1;
    int growEnd = -1;
    double growScale = 1.0;
    int word = -1;
    if (gazeValid) {
        word = selectablePhraseWord(m_caret.words, gaze, caretBoldWord(), buffer.selectionStart(),
                                    buffer.selectionEnd());
        const PhraseWordStep step =
            stepPhraseWord(m_caret.wordLook, word, dt, kPhraseWordScanMs, kPhraseWordDwellMs);
        m_caret.wordLook = step.look;
        if (step.fired) {
            selectCaretWord(word);
        } else if (word >= 0 && !m_caret.wordLook.latched && !m_caret.wordLook.scanning
                   && m_caret.wordLook.elapsedMs > 0.0) {
            growStart = m_caret.words.at(word).start;
            growEnd = m_caret.words.at(word).end;
            growScale = phraseWordGrowScale(m_caret.wordLook.elapsedMs, kPhraseWordDwellMs);
        }
    } else {
        m_caret.wordLook = {};
    }

    const bool ack = phraseCopyAckOn(m_caret.ack);
    const bool sameAsCopied = buffer.hasSelection() && buffer.selectionStart() == m_caret.copiedStart
                              && buffer.selectionEnd() == m_caret.copiedEnd;
    if (!buffer.hasSelection()) {
        m_caret.copiedStart = -1;
        m_caret.copiedEnd = -1;
    }
    const bool blockCopy = m_caret.host == CaretLook::Host::Phrase && capturingPhrase();
    const bool copyUp = buffer.hasSelection() && !blockCopy && !sameAsCopied;
    if ((!copyUp && !ack) || anchor.isNull()) {
        hideCaretCopy();
    } else {
        showCaretCopy(anchor);
        const int button = gazeValid && !ack && word < 0 && phraseCaretButtonAt(m_caret.button, gaze)
                               ? 0
                               : -1;
        const PhraseLookStep step = stepPhraseLook(m_caret.buttonLook, button, dt, kPhraseWordDwellMs);
        m_caret.buttonLook = step.look;
        if (step.fired && copy()) {
            m_caret.copiedStart = buffer.selectionStart();
            m_caret.copiedEnd = buffer.selectionEnd();
            const int gen = ++m_caret.ackGen;
            const CaretLook::Host host = m_caret.host;
            m_caret.ack.restart();
            QTimer::singleShot(static_cast<int>(kPhraseCopyAckMs), m_caretUi, [this, gen, host] {
                if (gen != m_caret.ackGen || m_caret.host != host) {
                    return;
                }
                m_caret.ack.invalidate();
                const ComposeBuffer& live = caretBuffer();
                const bool same = live.hasSelection() && live.selectionStart() == m_caret.copiedStart
                                  && live.selectionEnd() == m_caret.copiedEnd;
                const bool blocked = m_caret.host == CaretLook::Host::Phrase && capturingPhrase();
                if (m_caret.shown && (!live.hasSelection() || blocked || same)) {
                    hideCaretCopy();
                }
                updateCaretChooser({}, false);
            });
        }
    }

    publishCaretGrow(caretTarget(), growStart, growEnd, growScale);
    updateCaretChooser(gaze, gazeValid);
}

void ComposeUi::feedCaret(const GazePoint& point)
{
    if (m_pages.isDwellSuspended()) {
        idleCaret(caretTarget());
        return;
    }
    const bool exportPage = hasLive(kFilesLiveId) && m_fileMode == FileMode::Export
                            && m_pages.topPageId() == kFilesLiveId;
    if (exportPage) {
        useCaretHost(CaretLook::Host::File);
        const PageTarget* name = fileNameTarget(m_pages);
        const QRectF cell = name != nullptr ? name->geom.contentOnScreen() : QRectF();
        const bool ready = exportCaretEngaged() && name != nullptr && !cell.isEmpty()
                           && name->label == m_fileName.text()
                           && frontPageAt(m_pages, cell) == kFilesLiveId;
        if (!ready) {
            idleCaret(name);
            return;
        }
        const QFont font(BoardPaint::segoeFamily(), kFileNameFontPx, QFont::Normal);
        const QRectF lineRect = fileNameLineRect(font, phrasePad(cell), !name->caption.isEmpty());
        const int flags = int(Qt::AlignLeft | Qt::AlignVCenter);
        const QVector<PhraseLineGeom> lines =
            layoutPhraseLines(lineRect, name->label, font, flags, PhraseWrap::None);
        m_caret.words =
            phraseWordSpans(caretLines(lines), alphanumericCaretTokens(m_fileName.text()));
        runCaret(point, cell);
        return;
    }

    useCaretHost(CaretLook::Host::Phrase);
    const PageTarget* phrase = phraseTarget(m_pages);
    if (!isOpen()) {
        idleCaret(phrase);
        return;
    }
    const QRectF cell = phrase != nullptr ? phrase->geom.contentOnScreen() : QRectF();
    const bool ready = phrase != nullptr && !cell.isEmpty() && phrase->label == m_buffer.text()
                       && frontPageAt(m_pages, cell) == kPageId;
    if (!ready) {
        idleCaret(phrase);
        return;
    }
    const PhraseTextFrame frame =
        phraseTextFrame(BoardPaint::segoeFamily(), cell, phrase->label, phrase->caption,
                        phrase->textStyle, phrase->role, true, false);
    if (!frame.valid) {
        idleCaret(phrase);
        return;
    }
    const QVector<PhraseLineGeom> lines =
        layoutPhraseLines(frame.textRect, phrase->label, frame.font, frame.flags);
    m_caret.words = phraseWordSpans(caretLines(lines), alphanumericCaretTokens(m_buffer.text()));
    if (PageHostWindow* window = m_pages.window(); window != nullptr && phrase->caretIndex >= 0) {
        const int line = phraseBoundaryCaretLine(caretLines(lines), phrase->caretIndex);
        if (phrase->caretLine != line) {
            window->setTargetCaret(sessionKey(*phrase), phrase->caretIndex, line,
                                   phrase->selectionStart, phrase->selectionEnd);
        }
    }
    runCaret(point, cell);
}

void ComposeUi::layoutPhraseBox(PageDocument& doc) const
{
    if (doc.id != kPageId) {
        return;
    }
    PageGrid* stage = doc.findGrid(QStringLiteral("stage"));
    const PageGrid* authStage = m_composeAuthored.findGrid(QStringLiteral("stage"));
    const PageGrid* authCompose = m_composeAuthored.findGrid(QStringLiteral("compose"));
    const PageCell* phrase = doc.findCell(QStringLiteral("phrase"));
    if (stage == nullptr || authStage == nullptr || authCompose == nullptr || phrase == nullptr) {
        return;
    }
    const int composeRow = authCompose->row;
    if (composeRow < 0 || composeRow >= authStage->rowTracks.size()) {
        return;
    }
    const PageTrackSize& authored = authStage->rowTracks.at(composeRow);
    const double basePx = (!authored.isStar() && authored.dim.unit == PageDim::Unit::Pixels)
                              ? authored.dim.value
                              : 96.0;
    while (stage->rowTracks.size() <= composeRow) {
        stage->rowTracks.push_back(PageTrackSize::starWeight(1.0));
    }
    stage->rowTracks[composeRow] = authored;

    double cellW = 0.0;
    for (const PageTarget& t : m_pages.targets()) {
        if (t.pageId == kPageId && localIdOf(t) == QLatin1String("phrase")) {
            cellW = t.geom.contentOnScreen().width();
            break;
        }
    }
    if (cellW < 40.0) {
        return;
    }

    const PhraseStyle face = phraseStyle(phrase->textStyle, phrase->role);
    const QFont font(BoardPaint::segoeFamily(), face.px, face.weight);
    const double textW = qMax(1.0, cellW - 2.0 * kPhrasePadX);
    const double block = phraseBlockHeight(phrase->label, font, textW, face.flags);
    double chrome = authCompose->marginPx * 2.0 + kPhrasePadY * 2.0;
    if (!phrase->caption.isEmpty()) {
        chrome += QFontMetricsF(QFont(BoardPaint::segoeFamily(), 12)).lineSpacing() + 2.0;
    }
    double rowPx = phraseRowPx(basePx, block, chrome);

    double limit = rowPx;
    for (const PageGridPaint& g : m_pages.gridPaints()) {
        if (g.pageId != kPageId || g.gridId != QLatin1String("stage") || g.visual.height() < 1.0) {
            continue;
        }
        double others = 0.0;
        const int rows = qMax(1, authStage->rows);
        for (int i = 0; i < rows; ++i) {
            if (i == composeRow || i >= authStage->rowTracks.size()) {
                continue;
            }
            const PageTrackSize& track = authStage->rowTracks.at(i);
            if (!track.isStar() && track.dim.unit == PageDim::Unit::Pixels) {
                others += track.dim.value;
            }
        }
        const double inner = g.visual.height() - authStage->marginPx * 2.0
                             - authStage->gapPx * qMax(0, rows - 1);
        limit = qMax(basePx, inner - others);
        break;
    }
    rowPx = qMin(rowPx, limit);
    if (qAbs(rowPx - basePx) > 0.5) {
        stage->rowTracks[composeRow] = PageTrackSize::fromDim(PageDim::pixels(rowPx));
    }
}

} // namespace gazer
