#include "app/ComposeUi.h"

#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/BoardPaint.h"
#include "ui/OverlaySurface.h"
#include "ui/PageHostWindow.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QScreen>

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

QColor phraseCopyBorder(const PageHostWindow* window)
{
    if (window != nullptr && window->theme().accent.isValid()) {
        return window->theme().accent;
    }
    return QColor(0x8a, 0xb4, 0xf8);
}

const QColor kPhraseButtonDisc(28, 32, 40);

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

} // namespace

class PhraseStyleChipOverlay final : public OverlaySurface {
public:
    struct View {
        bool shown = false;
        QRectF rect;
        QString label;
        bool hot = false;
        double progress = 0.0;
        QColor border;
        QColor ink;
    };

    void setView(const View& view)
    {
        m_view = view;
        if (!view.shown || view.rect.isEmpty()) {
            hide();
            return;
        }
        QRectF box = view.rect;
        box.adjust(-18.0, -18.0, 18.0, 18.0);
        setGeometry(box.toAlignedRect());
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_view.shown || m_view.rect.isEmpty()) {
            return;
        }
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box = m_view.rect.translated(-pos());
        const double radius = box.height() * 0.5;
        const QColor border = m_view.border.isValid() ? m_view.border : QColor(0x8a, 0xb4, 0xf8);
        const QColor ink = m_view.ink.isValid() ? m_view.ink : QColor(255, 255, 255);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 80));
        p.drawRoundedRect(box.translated(0, 2), radius, radius);
        p.setPen(QPen(border, kPhraseCopyBorderPx));
        p.setBrush(m_view.hot ? QColor(48, 54, 64, 242)
                              : QColor(kPhraseButtonDisc.red(), kPhraseButtonDisc.green(),
                                       kPhraseButtonDisc.blue(), 235));
        p.drawRoundedRect(box, radius, radius);
        if (m_view.hot && m_view.progress > 0.0) {
            paintChoiceRing(p, box.center(), radius + 8.0, m_view.progress, ink);
        }
        p.setPen(QColor(255, 255, 255, 235));
        QFont font(BoardPaint::segoeFamily(), 15, QFont::DemiBold);
        p.setFont(font);
        const QString text = QFontMetrics(font).elidedText(m_view.label, Qt::ElideRight,
                                                           int(qMax(1.0, box.width() - 24.0)));
        p.drawText(box.adjusted(12, 0, -12, 0), Qt::AlignCenter, text);
    }

private:
    View m_view;
};

void ComposeUi::destroyStyleChip()
{
    delete m_styleChipUi;
    m_styleChipUi = nullptr;
}

QString ComposeUi::styleChipLabel() const
{
    QString voice = QStringLiteral("Voice");
    const QString id = currentVoiceId();
    if (!elevenMode() && id.isEmpty()) {
        voice = QStringLiteral("Windows");
    } else {
        for (const VoiceCatalog::Voice& v : currentVoices()) {
            if (v.id == id && !v.name.trimmed().isEmpty()) {
                voice = v.name.trimmed();
                break;
            }
        }
    }
    const QString style = VoiceStyles::normalizeStyleName(m_settings.speechStyle);
    return voice + QStringLiteral(" · ") + (style.isEmpty() ? QStringLiteral("None") : style);
}

void ComposeUi::refreshStyleChip(bool hot)
{
    if (!m_styleChipShown || m_styleChip.rect.isEmpty()) {
        if (m_styleChipUi != nullptr) {
            m_styleChipUi->hide();
        }
        return;
    }
    if (m_styleChipUi == nullptr) {
        m_styleChipUi = new PhraseStyleChipOverlay();
    }
    PhraseStyleChipOverlay::View view;
    view.shown = true;
    view.rect = m_styleChip.rect;
    view.label = m_styleChipLabel;
    view.hot = hot;
    view.ink = QColor(m_settings.progressColor);
    view.border = phraseCopyBorder(m_pages.window());
    if (hot && !m_styleChipLook.latched) {
        view.progress = m_styleChipLook.elapsedMs / kPhraseWordDwellMs;
    }
    m_styleChipUi->setView(view);
}

void ComposeUi::feedStyleChip(const GazePoint& point)
{
    qint64 now = point.timestampMs;
    if (now <= 0) {
        if (!m_styleChipClock.isValid()) {
            m_styleChipClock.start();
        }
        now = m_styleChipClock.elapsed();
    }
    double dt = 0.0;
    if (m_styleChipLookMs >= 0 && now > m_styleChipLookMs) {
        dt = qBound(0.0, double(now - m_styleChipLookMs), 80.0);
    }
    m_styleChipLookMs = now;

    const bool exportPage = hasLive(kFilesLiveId) && m_fileMode == FileMode::Export
                            && m_pages.topPageId() == kFilesLiveId;
    const PageTarget* phrase = phraseTarget(m_pages);
    const QRectF cell = phrase != nullptr ? phrase->geom.contentOnScreen() : QRectF();
    const bool show = isOpen() && !capturingPhrase() && !exportPage && phrase != nullptr && !cell.isEmpty()
                      && frontPageAt(m_pages, cell) == kPageId;
    if (!show) {
        m_styleChip = {};
        m_styleChipLook = {};
        m_styleChipShown = false;
        refreshStyleChip(false);
        return;
    }
    const QString key = currentVoiceId() + QLatin1Char('|') + m_settings.speechStyle + QLatin1Char('|')
                        + m_settings.speechModel;
    if (key != m_styleChipKey || m_styleChipLabel.isEmpty()) {
        m_styleChipKey = key;
        m_styleChipLabel = styleChipLabel();
    }
    const QFont font(BoardPaint::segoeFamily(), 15, QFont::DemiBold);
    const double advance = QFontMetricsF(font).horizontalAdvance(m_styleChipLabel);
    layoutPhraseStyleChip(cell, screenAround(cell.center()), advance, m_styleChip);
    m_styleChipShown = !m_styleChip.rect.isEmpty();
    const bool onChip = point.valid && m_styleChipShown && phraseStyleChipAt(m_styleChip, point.toPointF());
    const bool suspended = m_pages.isDwellSuspended();
    const StyleChipTick tick =
        stepStyleChip(m_styleChipShown, onChip, suspended, m_styleChipLook, dt);
    m_styleChipLook = tick.look;
    if (tick.fired) {
        m_styleChipShown = false;
        m_styleChip = {};
        m_styleChipLook = {};
        refreshStyleChip(false);
        QString err;
        if (!openStyles(&err)) {
            notify(err.isEmpty() ? QStringLiteral("Could not open styles") : err);
        }
        return;
    }
    if (!m_styleChipShown) {
        m_styleChipLook = {};
    }
    refreshStyleChip(tick.hot);
}

} // namespace gazer
