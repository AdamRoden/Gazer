#include "editor/LayoutEditorCanvas.h"

#include "layout/DwellRegionSpace.h"
#include "layout/LayoutGeometry.h"
#include "ui/LayoutBoardPainter.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QHash>
#include <QInputDialog>
#include <QLineEdit>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>

namespace gazer {

LayoutEditorCanvas::LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    setMinimumSize(480, 360);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    m_testTimer = new QTimer(this);
    m_testTimer->setInterval(30);
    connect(m_testTimer, &QTimer::timeout, this, [this]() { tickTestDwell(); });
    connect(&m_session, &LayoutEditorSession::documentChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::selectionChanged, this, QOverload<>::of(&QWidget::update));
    connect(&m_session, &LayoutEditorSession::placeKindChanged, this, [this]() {
        setCursor(m_session.placeKind() ? Qt::CrossCursor : Qt::ArrowCursor);
        update();
    });
}

void LayoutEditorCanvas::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    update();
}

void LayoutEditorCanvas::setShowGrid(bool on)
{
    m_showGrid = on;
    update();
}

void LayoutEditorCanvas::setFitBoard(bool on)
{
    m_fitBoard = on;
    update();
}

void LayoutEditorCanvas::setTestMode(bool on)
{
    m_testMode = on;
    m_testProgress = 0.0;
    if (on) {
        m_testTimer->start();
    } else {
        m_testTimer->stop();
    }
    update();
}

void LayoutEditorCanvas::tickTestDwell()
{
    if (!m_testMode) {
        return;
    }
    if (m_hoverId.isEmpty()) {
        m_testProgress = 0.0;
        update();
        return;
    }
    int dwellMs = 800;
    if (const LayoutItem* it = m_session.itemById(m_hoverId)) {
        const QVector<int> seq =
            LayoutDwellConfig::resolveSequence(&it->dwell, m_session.document().dwell, {});
        if (!seq.isEmpty()) {
            dwellMs = qMax(50, seq.first());
        }
    }
    m_testProgress = qMin(1.0, m_testProgress + (30.0 / double(dwellMs)));
    if (m_testProgress >= 1.0) {
        if (const LayoutItem* it = m_session.itemById(m_hoverId)) {
            const auto acts = it->effectiveActions();
            QStringList parts;
            for (const LayoutAction& a : acts) {
                if (a.type == LayoutAction::Type::TypeText) {
                    parts.push_back(QStringLiteral("type “%1”").arg(a.text));
                } else if (a.type == LayoutAction::Type::Speak) {
                    parts.push_back(QStringLiteral("speak “%1”").arg(a.text));
                } else if (a.type == LayoutAction::Type::Command) {
                    parts.push_back(a.name);
                } else if (a.type == LayoutAction::Type::OpenLayout) {
                    parts.push_back(QStringLiteral("open %1").arg(a.layoutId));
                } else if (a.type == LayoutAction::Type::LoadLayout) {
                    parts.push_back(QStringLiteral("load %1").arg(a.layoutId));
                } else if (a.type == LayoutAction::Type::CloseLayout) {
                    parts.push_back(QStringLiteral("close"));
                } else if (a.type == LayoutAction::Type::Script) {
                    parts.push_back(QStringLiteral("script"));
                }
            }
            QString msg = it->label.isEmpty() ? it->id : it->label;
            if (!parts.isEmpty()) {
                msg = QStringLiteral("%1 → %2").arg(msg, parts.join(QStringLiteral(", ")));
            }
            m_session.notify(msg);
        }
        m_testProgress = 0.0;
    }
    update();
}

LayoutEditorCanvas::ScreenMap LayoutEditorCanvas::map() const
{
    ScreenMap out;
    out.virtualScreen = QSize(1920, 1080);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize sz = screen->size();
        if (sz.width() >= 640 && sz.height() >= 360) {
            out.virtualScreen = sz;
        }
    }
    const QRectF canvas = QRectF(rect());
    if (canvas.width() < 40 || canvas.height() < 40) {
        return out;
    }

    const QRect virt(0, 0, out.virtualScreen.width(), out.virtualScreen.height());
    const LayoutDocument& doc = m_session.document();
    QRect boardVirt = LayoutGeometry::boardRectInBounds(doc, virt);

    int padL = 0;
    int padT = 0;
    int padR = 0;
    int padB = 0;
    if (!m_fitBoard) {
        padL = padT = padR = padB = 80;
        auto grow = [&](const QRect& r) {
            if (!r.isValid()) {
                return;
            }
            padL = qMax(padL, virt.left() - r.left());
            padT = qMax(padT, virt.top() - r.top());
            padR = qMax(padR, r.right() - virt.right());
            padB = qMax(padB, r.bottom() - virt.bottom());
        };
        if (doc.showsBoardWindow()) {
            grow(boardVirt);
        }
        for (const LayoutItem& item : doc.items) {
            if (item.isUnbounded()) {
                grow(DwellRegionSpace::logicalFromAnchor(item.dwellRegion, virt));
            }
        }
        padL = qMin(padL, 480);
        padT = qMin(padT, 480);
        padR = qMin(padR, 480);
        padB = qMin(padB, 480);
    }

    const QRect virtView(virt.left() - padL, virt.top() - padT, virt.width() + padL + padR,
                         virt.height() + padT + padB);
    QRectF usable = canvas;
    if (!m_fitBoard) {
        constexpr qreal kBezel = 14.0;
        constexpr qreal kChin = 22.0;
        constexpr qreal kStand = 26.0;
        constexpr qreal kCaption = 18.0;
        usable = canvas.adjusted(kBezel + 8, kBezel + 8, -(kBezel + 8),
                                 -(kChin + kStand + kCaption + 8));
    }
    if (m_fitBoard) {
        QRect target = (doc.showsBoardWindow() && boardVirt.isValid() && !boardVirt.isEmpty())
                           ? boardVirt
                           : virt;
        if (target.width() < 1) {
            target.setWidth(1);
        }
        if (target.height() < 1) {
            target.setHeight(1);
        }
        constexpr qreal kPad = 12.0;
        const QRectF fit = canvas.adjusted(kPad, kPad, -kPad, -kPad);
        const qreal s = qMin(fit.width() / qMax(1.0, double(target.width())),
                             fit.height() / qMax(1.0, double(target.height())));
        out.scaleX = s;
        out.scaleY = s;
        const qreal tw = target.width() * s;
        const qreal th = target.height() * s;
        const QPointF topLeft(canvas.center().x() - tw / 2.0, canvas.center().y() - th / 2.0);
        out.screen = QRectF(topLeft.x() - target.x() * s, topLeft.y() - target.y() * s,
                            virt.width() * s, virt.height() * s);
        out.glass = out.screen;
        out.bezel = out.screen;
    } else {
        const qreal s = qMin(usable.width() / qMax(1.0, double(virtView.width())),
                             usable.height() / qMax(1.0, double(virtView.height())));
        out.scaleX = s;
        out.scaleY = s;
        const qreal vw = virtView.width() * s;
        const qreal vh = virtView.height() * s;
        const QPointF origin(usable.center().x() - vw / 2.0, usable.center().y() - vh / 2.0);
        out.screen = QRectF(origin.x() + padL * s, origin.y() + padT * s, virt.width() * s,
                            virt.height() * s);
        out.glass = out.screen;
        out.bezel = out.screen.adjusted(-14.0, -14.0, 14.0, 22.0);
    }

    if (m_drag == Drag::Window && doc.showsBoardWindow()) {
        const QPointF delta = m_dragPos - m_pressPos;
        boardVirt.translate(int(delta.x() / qMax(0.001, out.scaleX)),
                            int(delta.y() / qMax(0.001, out.scaleY)));
    }
    out.board = doc.showsBoardWindow() ? out.fromVirt(boardVirt) : QRectF{};
    out.virtBoard = boardVirt.isValid() ? boardVirt.size() : QSize(1, 1);
    if (out.virtBoard.width() < 1) {
        out.virtBoard.setWidth(1);
    }
    if (out.virtBoard.height() < 1) {
        out.virtBoard.setHeight(1);
    }
    return out;
}

QHash<QString, QRectF> LayoutEditorCanvas::boardItemRects(const ScreenMap& m) const
{
    const auto virtRects =
        LayoutGeometry::itemRects(m_session.document(), m.virtBoard.width(), m.virtBoard.height());
    QHash<QString, QRectF> out;
    out.reserve(virtRects.size());
    for (auto it = virtRects.constBegin(); it != virtRects.constEnd(); ++it) {
        const QRectF& r = it.value();
        out.insert(it.key(), QRectF(r.x() * m.scaleX, r.y() * m.scaleY, r.width() * m.scaleX,
                                    r.height() * m.scaleY));
    }
    return out;
}

QRectF LayoutEditorCanvas::unboundedCanvasRect(const LayoutItem& item, const ScreenMap& m) const
{
    if (!item.isUnbounded()) {
        return {};
    }
    const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
    const QRect logical = DwellRegionSpace::logicalFromAnchor(item.dwellRegion, virt);
    if (!logical.isValid() || logical.isEmpty()) {
        return {};
    }
    return m.fromVirt(logical);
}

QString LayoutEditorCanvas::hitUnbounded(const QPoint& pos, const ScreenMap& m) const
{
    QString best;
    double bestArea = 1e18;
    for (const LayoutItem& item : m_session.document().items) {
        if (item.participatesInBoardGrid()) {
            continue;
        }
        const QRectF r = unboundedCanvasRect(item, m);
        if (r.contains(pos)) {
            const double area = r.width() * r.height();
            if (area < bestArea) {
                bestArea = area;
                best = item.id;
            }
        }
    }
    return best;
}

QString LayoutEditorCanvas::hitItem(const QPoint& pos, const ScreenMap& m) const
{
    if (const QString edge = hitUnbounded(pos, m); !edge.isEmpty()) {
        return edge;
    }
    if (!m.board.isValid()) {
        return {};
    }
    const LayoutDocument& doc = m_session.document();
    const QHash<QString, QRectF> rects = boardItemRects(m);
    const QPointF local(pos.x() - m.board.x(), pos.y() - m.board.y());
    QString best;
    double bestArea = 1e18;
    for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
        if (it.value().contains(local)) {
            const double area = it.value().width() * it.value().height();
            if (area < bestArea) {
                bestArea = area;
                best = it.key();
            }
        }
    }
    return best;
}

bool LayoutEditorCanvas::hitBoard(const QPoint& pos, const ScreenMap& m) const
{
    return m.board.contains(pos);
}

QPoint LayoutEditorCanvas::cellAt(const QPoint& pos, const ScreenMap& m) const
{
    const LayoutDocument& doc = m_session.document();
    const int cols = qMax(1, doc.grid.columns);
    const int rows = qMax(1, doc.grid.rows);
    const QPointF local((pos.x() - m.board.x()) / qMax(0.001, m.scaleX),
                        (pos.y() - m.board.y()) / qMax(0.001, m.scaleY));
    const auto inset = doc.grid.insets(m.virtBoard.width(), m.virtBoard.height());
    const double innerW = m.virtBoard.width() - inset.left - inset.right;
    const double innerH = m.virtBoard.height() - inset.top - inset.bottom;
    int col = int((local.x() - inset.left) / qMax(1.0, innerW / cols));
    int row = int((local.y() - inset.top) / qMax(1.0, innerH / rows));
    col = qBound(0, col, cols - 1);
    row = qBound(0, row, rows - 1);
    return {col, row};
}

void LayoutEditorCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), m_theme.bgMain);
    const ScreenMap m = map();
    if (!m.screen.isValid()) {
        return;
    }
    paintMonitor(p, m);
    paintBoard(p, m);
    if (m_fitBoard) {
        paintPlacementPip(p, m);
    }
    if (m_drag == Drag::Rubber && !m_rubber.isEmpty()) {
        p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
        QColor fill = m_theme.accent;
        fill.setAlpha(40);
        p.setBrush(fill);
        p.drawRect(m_rubber.normalized());
    }
    if (m_session.placeKind()) {
        p.setPen(m_theme.textSecondary);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 10));
        const QString hint = *m_session.placeKind() == EditorItemKind::Unbounded
                                 ? QStringLiteral("Click to place a free item  ·  Esc cancel")
                                 : QStringLiteral("Click a cell to place  ·  Esc cancel");
        p.drawText(rect().adjusted(12, 8, -12, -8), Qt::AlignTop | Qt::AlignHCenter, hint);
    }
}

void LayoutEditorCanvas::paintPlacementPip(QPainter& p, const ScreenMap& m) const
{
    const QRectF pip(m.screen.right() - 132, m.screen.bottom() - 82, 120, 68);
    p.setPen(QPen(m_theme.border, 1));
    QColor pipBg = m_theme.bgSurface;
    pipBg.setAlpha(220);
    p.setBrush(pipBg);
    p.drawRoundedRect(pip, 4, 4);
    const LayoutDocument& doc = m_session.document();
    const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
    const QRect board = LayoutGeometry::boardRectInBounds(doc, virt);
    const qreal sx = pip.width() / double(m.virtualScreen.width());
    const qreal sy = pip.height() / double(m.virtualScreen.height());
    const QRectF br(pip.left() + board.x() * sx, pip.top() + board.y() * sy, board.width() * sx,
                    board.height() * sy);
    QColor pipBoard = m_theme.accent;
    pipBoard.setAlpha(140);
    p.setBrush(pipBoard);
    p.setPen(Qt::NoPen);
    p.drawRect(br);
    p.setPen(m_theme.textSecondary);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 7));
    p.drawText(pip.adjusted(4, 2, -4, -2), Qt::AlignTop | Qt::AlignLeft, QStringLiteral("Placement"));
}

void LayoutEditorCanvas::paintMonitor(QPainter& p, const ScreenMap& m) const
{
    if (!m_fitBoard) {
        QPainterPath bezel;
        bezel.addRoundedRect(m.bezel, 18, 18);
        p.fillPath(bezel, m_theme.bgSurface);
        p.setPen(QPen(m_theme.border, 1.2));
        p.drawPath(bezel);

        const QRectF standNeck(m.bezel.center().x() - 18, m.bezel.bottom() - 4, 36, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(m_theme.border);
        p.drawRoundedRect(standNeck, 3, 3);
        const QRectF standBase(m.bezel.center().x() - 70, standNeck.bottom() - 2, 140, 10);
        p.drawRoundedRect(standBase, 5, 5);
    }

    QLinearGradient wall(m.screen.topLeft(), m.screen.bottomRight());
    wall.setColorAt(0.0, m_theme.bgMain);
    wall.setColorAt(0.55, m_theme.bgSurface);
    wall.setColorAt(1.0, m_theme.bgSurfaceActive);
    QPainterPath glass;
    glass.addRoundedRect(m.screen, m_fitBoard ? 0 : 4, m_fitBoard ? 0 : 4);
    p.fillPath(glass, wall);

    if (!m_fitBoard) {
        p.setPen(m_theme.textSecondary);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        const LayoutDocument& doc = m_session.document();
        const QString caption =
            QStringLiteral("%1  ·  %2×%3  ·  grid %4×%5")
                .arg(doc.name.isEmpty() ? doc.id : doc.name)
                .arg(int(LayoutGeometry::boardRectInBounds(
                             doc, QRect(0, 0, m.virtualScreen.width(), m.virtualScreen.height()))
                             .width()))
                .arg(int(LayoutGeometry::boardRectInBounds(
                             doc, QRect(0, 0, m.virtualScreen.width(), m.virtualScreen.height()))
                             .height()))
                .arg(doc.grid.columns)
                .arg(doc.grid.rows);
        p.drawText(QRectF(m.bezel.left(), m.bezel.bottom() + 14, m.bezel.width(), 18),
                   Qt::AlignHCenter | Qt::AlignTop, caption);
    }
}

void LayoutEditorCanvas::paintBoardItems(QPainter& p, const ScreenMap& m,
                                         const QHash<QString, QRectF>& virtRects,
                                         const QRectF& origin, int virtW, int virtH,
                                         bool windowChrome) const
{
    if (virtW < 1 || virtH < 1 || origin.width() < 2 || origin.height() < 2) {
        return;
    }
    LayoutBoardPainter::Input in;
    in.layout = &m_session.document();
    in.itemLocalRects = &virtRects;
    in.theme = &m_theme;
    in.width = virtW;
    in.height = virtH;
    in.hoverId = m_hoverId;
    in.hoverProgress = m_testMode ? m_testProgress : 0.0;
    in.eraseBackground = false;
    in.paintWindowChrome = windowChrome;
    p.save();
    p.translate(origin.topLeft());
    p.scale(m.scaleX, m.scaleY);
    LayoutBoardPainter(in).paint(p);
    p.restore();
}

void LayoutEditorCanvas::paintSelectionOverlay(QPainter& p, const QRectF& r, bool handles) const
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_theme.accent, 1, Qt::DashLine));
    p.drawRect(r);
    if (handles) {
        paintHandles(p, r);
    }
}

void LayoutEditorCanvas::paintBoard(QPainter& p, const ScreenMap& m) const
{
    const LayoutDocument& doc = m_session.document();
    const QRectF board = m.board;
    const bool paintGrid = board.width() >= 4 && board.height() >= 4;
    const QHash<QString, QRectF> canvasRects = paintGrid ? boardItemRects(m) : QHash<QString, QRectF>{};

    if (paintGrid) {
        auto virtRects =
            LayoutGeometry::itemRects(doc, m.virtBoard.width(), m.virtBoard.height());
        if (m_drag != Drag::None && !m_dragId.isEmpty()
            && (m_dragPos - m_pressPos).manhattanLength() > 8) {
            const LayoutItem* item = m_session.itemById(m_dragId);
            if (item && item->participatesInBoardGrid() && virtRects.contains(item->id)) {
                QRectF r = virtRects.value(item->id);
                if (m_drag == Drag::Move) {
                    const QPoint dest = cellAt(m_dragPos, m);
                    const double cw = r.width() / qMax(1, item->colSpan);
                    const double rh = r.height() / qMax(1, item->rowSpan);
                    r.translate((dest.x() - item->col) * cw, (dest.y() - item->row) * rh);
                } else if (m_drag == Drag::ResizeW) {
                    r.setWidth(qMax(8.0, (m_dragPos.x() - m.board.x()) / qMax(0.001, m.scaleX)
                                             - r.left()));
                } else if (m_drag == Drag::ResizeH) {
                    r.setHeight(qMax(8.0, (m_dragPos.y() - m.board.y()) / qMax(0.001, m.scaleY)
                                              - r.top()));
                }
                virtRects.insert(item->id, r);
            }
        }
        paintBoardItems(p, m, virtRects, board, m.virtBoard.width(), m.virtBoard.height(), true);

        const bool windowSel = m_session.selection().target == EditorTarget::Window
                               || m_session.selection().target == EditorTarget::Grid;
        if (windowSel) {
            const double winR = doc.placement.style.radius.value_or(16.0) * m.px();
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(m_theme.accent, 2.0));
            p.drawRoundedRect(board.adjusted(-3, -3, 3, 3), winR + 2, winR + 2);
        }

        if (m_showGrid) {
            p.save();
            p.translate(board.topLeft());
            QColor gridC = m_theme.text;
            gridC.setAlpha(m_theme.bgMain.lightness() > 140 ? 50 : 32);
            p.setPen(QPen(gridC, 1, Qt::DashLine));
            const auto inset = doc.grid.insets(m.virtBoard.width(), m.virtBoard.height());
            const int cols = qMax(1, doc.grid.columns);
            const int rows = qMax(1, doc.grid.rows);
            const double px = m.px();
            const double innerW = (m.virtBoard.width() - inset.left - inset.right) * px;
            const double innerH = (m.virtBoard.height() - inset.top - inset.bottom) * px;
            const double left = inset.left * px;
            const double top = inset.top * px;
            const double cw = innerW / cols;
            const double rh = innerH / rows;
            for (int c = 0; c <= cols; ++c) {
                const double x = left + c * cw;
                p.drawLine(QPointF(x, top), QPointF(x, top + innerH));
            }
            for (int r = 0; r <= rows; ++r) {
                const double y = top + r * rh;
                p.drawLine(QPointF(left, y), QPointF(left + innerW, y));
            }
            p.restore();
        }

        if (!m_testMode) {
            p.save();
            p.translate(board.topLeft());
            const QString primary = m_session.selection().itemId;
            for (auto it = canvasRects.constBegin(); it != canvasRects.constEnd(); ++it) {
                if (!m_session.isItemSelected(it.key())) {
                    continue;
                }
                paintSelectionOverlay(p, it.value(), it.key() == primary);
            }
            p.restore();
        }
    }

    QHash<QString, QRectF> freeVirt;
    const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
    for (const LayoutItem& item : doc.items) {
        if (!item.isUnbounded()) {
            continue;
        }
        QRect logical = DwellRegionSpace::logicalFromAnchor(item.dwellRegion, virt);
        if (!logical.isValid() || logical.isEmpty()) {
            continue;
        }
        if (m_drag == Drag::Move && item.id == m_dragId
            && (m_dragPos - m_pressPos).manhattanLength() > 8) {
            logical.translate(int((m_dragPos.x() - m_pressPos.x()) / qMax(0.001, m.scaleX)),
                              int((m_dragPos.y() - m_pressPos.y()) / qMax(0.001, m.scaleY)));
        }
        freeVirt.insert(item.id, QRectF(logical));
    }
    if (!freeVirt.isEmpty()) {
        paintBoardItems(p, m, freeVirt, m.screen, m.virtualScreen.width(), m.virtualScreen.height(),
                        false);
        if (!m_testMode) {
            const QString primary = m_session.selection().itemId;
            for (const LayoutItem& item : doc.items) {
                if (!item.isUnbounded() || !m_session.isItemSelected(item.id)) {
                    continue;
                }
                const QRectF marker = unboundedCanvasRect(item, m);
                if (marker.isEmpty()) {
                    continue;
                }
                paintSelectionOverlay(p, marker, item.id == primary);
            }
        }
    }
}

void LayoutEditorCanvas::paintHandles(QPainter& p, const QRectF& r) const
{
    const QRectF hr(r.right() - 5, r.center().y() - 5, 10, 10);
    const QRectF hb(r.center().x() - 5, r.bottom() - 5, 10, 10);
    p.setBrush(m_theme.accent);
    p.setPen(Qt::NoPen);
    p.drawRect(hr);
    p.drawRect(hb);
}

QRectF LayoutEditorCanvas::selectedRect(const ScreenMap& m) const
{
    const QString id = m_session.selection().itemId;
    if (id.isEmpty()) {
        return {};
    }
    const LayoutItem* item = m_session.itemById(id);
    if (!item) {
        return {};
    }
    if (item->isUnbounded()) {
        return unboundedCanvasRect(*item, m);
    }
    return boardItemRects(m).value(id).translated(m.board.topLeft());
}

QPoint LayoutEditorCanvas::virtFromCanvas(const QPoint& pos, const ScreenMap& m) const
{
    return {int((pos.x() - m.screen.left()) / qMax(0.001, m.scaleX)),
            int((pos.y() - m.screen.top()) / qMax(0.001, m.scaleY))};
}

LayoutEditorCanvas::Drag LayoutEditorCanvas::hitHandle(const QPoint& pos, const ScreenMap& m) const
{
    const QRectF r = selectedRect(m);
    if (r.isEmpty()) {
        return Drag::None;
    }
    const QRectF hr(r.right() - 7, r.center().y() - 7, 14, 14);
    const QRectF hb(r.center().x() - 7, r.bottom() - 7, 14, 14);
    if (hr.contains(pos)) {
        return Drag::ResizeW;
    }
    if (hb.contains(pos)) {
        return Drag::ResizeH;
    }
    return Drag::None;
}

void LayoutEditorCanvas::updateDragCursor(const ScreenMap& m, const QPoint& pos)
{
    if (m_drag == Drag::ResizeW || hitHandle(pos, m) == Drag::ResizeW) {
        setCursor(Qt::SizeHorCursor);
    } else if (m_drag == Drag::ResizeH || hitHandle(pos, m) == Drag::ResizeH) {
        setCursor(Qt::SizeVerCursor);
    } else if (m_drag == Drag::Window) {
        setCursor(Qt::SizeAllCursor);
    } else if (m_session.placeKind()) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void LayoutEditorCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    const ScreenMap m = map();
    m_pressPos = event->pos();
    m_dragPos = event->pos();
    m_drag = Drag::None;
    m_dragId.clear();

    const Drag handle = hitHandle(event->pos(), m);
    if (handle != Drag::None) {
        m_drag = handle;
        m_dragId = m_session.selection().itemId;
        m_pressRect = boardItemRects(m).value(m_dragId);
        return;
    }

    const QString id = hitItem(event->pos(), m);
    if (!id.isEmpty()) {
        m_drag = Drag::Move;
        m_dragId = id;
        m_session.selectItem(id, event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
        return;
    }

    if (m_session.placeKind() && *m_session.placeKind() == EditorItemKind::Unbounded) {
        m_session.addFreeItemAt(virtFromCanvas(event->pos(), m));
        return;
    }
    if (!hitBoard(event->pos(), m)) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    if (m_session.placeKind()) {
        const QPoint cell = cellAt(event->pos(), m);
        m_session.addItemAt(*m_session.placeKind(), cell.y(), cell.x());
        return;
    }
    if (!m_fitBoard) {
        m_drag = Drag::Window;
        return;
    }
    m_drag = Drag::Rubber;
    m_rubber = QRect(event->pos(), QSize(1, 1));
}

void LayoutEditorCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const ScreenMap m = map();
    m_dragPos = event->pos();
    const QString hover = hitItem(event->pos(), m);
    if (hover != m_hoverId) {
        m_hoverId = hover;
        m_testProgress = 0.0;
    }
    if (m_drag == Drag::Rubber) {
        m_rubber = QRect(m_pressPos, event->pos()).normalized();
    }
    updateDragCursor(m, event->pos());
    update();
}

void LayoutEditorCanvas::commitDrag(const QPoint& pos, const ScreenMap& m)
{
    switch (m_drag) {
    case Drag::Move:
        if (!m_dragId.isEmpty() && (pos - m_pressPos).manhattanLength() > 8) {
            const LayoutItem* it = m_session.itemById(m_dragId);
            if (it && it->isUnbounded()) {
                const double dx = (pos.x() - m_pressPos.x()) / qMax(0.001, m.scaleX);
                const double dy = (pos.y() - m_pressPos.y()) / qMax(0.001, m.scaleY);
                const QString id = m_dragId;
                m_session.edit(QStringLiteral("Move free item"), [&](LayoutDocument& d) {
                    for (LayoutItem& item : d.items) {
                        if (item.id != id) {
                            continue;
                        }
                        const double x0 =
                            item.dwellRegion.x.isSet() ? item.dwellRegion.x.value : 0.0;
                        const double y0 =
                            item.dwellRegion.y.isSet() ? item.dwellRegion.y.value : 0.0;
                        item.dwellRegion.x = DimSpec::pixels(x0 + dx);
                        item.dwellRegion.y = DimSpec::pixels(y0 + dy);
                        break;
                    }
                });
            } else if (m.board.contains(pos)) {
                const QPoint cell = cellAt(pos, m);
                const int dRow = it ? cell.y() - it->row : 0;
                const int dCol = it ? cell.x() - it->col : 0;
                if (m_session.selection().itemIds.size() > 1) {
                    m_session.moveSelected(dRow, dCol);
                } else {
                    m_session.moveItemToCell(m_dragId, cell.y(), cell.x());
                }
            }
        }
        break;
    case Drag::ResizeW:
    case Drag::ResizeH:
        applyResize(pos, m);
        break;
    case Drag::Rubber: {
        const auto rects = boardItemRects(m);
        const QRectF band = QRectF(m_rubber.normalized()).translated(-m.board.topLeft());
        QStringList ids;
        for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
            if (it.value().intersects(band)) {
                ids.push_back(it.key());
            }
        }
        const QRectF bandCanvas = QRectF(m_rubber.normalized());
        for (const LayoutItem& item : m_session.document().items) {
            if (!item.isUnbounded()) {
                continue;
            }
            const QRectF r = unboundedCanvasRect(item, m);
            if (r.intersects(bandCanvas)) {
                ids.push_back(item.id);
            }
        }
        m_session.selectItems(ids);
        m_rubber = {};
        break;
    }
    case Drag::Window: {
        const QPointF delta = pos - m_pressPos;
        const QRect virt(0, 0, m.virtualScreen.width(), m.virtualScreen.height());
        QRect board = LayoutGeometry::boardRectInBounds(m_session.document(), virt);
        board.translate(int(delta.x() / qMax(0.001, m.scaleX)),
                        int(delta.y() / qMax(0.001, m.scaleY)));
        m_session.snapWindowTo(board.topLeft(), m.virtualScreen);
        break;
    }
    case Drag::None:
        break;
    }
}

void LayoutEditorCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    commitDrag(event->pos(), map());
    m_drag = Drag::None;
    m_dragId.clear();
    m_dragPos = event->pos();
    update();
}

void LayoutEditorCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    const LayoutItem* item = id.isEmpty() ? nullptr : m_session.itemById(id);
    if (!item) {
        return;
    }
    bool ok = false;
    const QString label = QInputDialog::getText(this, QStringLiteral("Key label"),
                                                QStringLiteral("Text"), QLineEdit::Normal,
                                                item->label, &ok);
    if (ok) {
        m_session.setItemLabel(id, label);
    }
}

void LayoutEditorCanvas::applyResize(const QPoint& pos, const ScreenMap& m)
{
    const LayoutItem* item = m_session.itemById(m_dragId);
    if (!item) {
        return;
    }
    if (item->isUnbounded()) {
        const QRectF r = unboundedCanvasRect(*item, m);
        if (r.width() < 1 || r.height() < 1) {
            return;
        }
        double w = item->dwellRegion.width.isSet()
                       ? item->dwellRegion.width.resolve(m.virtualScreen.width())
                       : r.width() / qMax(0.001, m.scaleX);
        double h = item->dwellRegion.height.isSet()
                       ? item->dwellRegion.height.resolve(m.virtualScreen.height())
                       : r.height() / qMax(0.001, m.scaleY);
        if (m_drag == Drag::ResizeW) {
            w = qMax(16.0, (pos.x() - r.left()) / qMax(0.001, m.scaleX));
        } else {
            h = qMax(16.0, (pos.y() - r.top()) / qMax(0.001, m.scaleY));
        }
        m_session.resizeFreeItem(item->id, DimSpec::pixels(w), DimSpec::pixels(h));
        return;
    }
    if (m_pressRect.width() < 1) {
        return;
    }
    const QPointF local(pos.x() - m.board.x(), pos.y() - m.board.y());
    if (m_drag == Drag::ResizeW) {
        const double newW = qMax(8.0, local.x() - m_pressRect.left());
        if (m_session.document().grid.unitRows || item->widthUnits > 0) {
            const double u = item->widthUnits > 0 ? item->widthUnits : 1.0;
            m_session.resizeItem(item->id, item->rowSpan, item->colSpan,
                                 qMax(0.3, u * (newW / m_pressRect.width())));
        } else {
            const double cellW = m_pressRect.width() / qMax(1, item->colSpan);
            m_session.resizeItem(item->id, item->rowSpan,
                                 qMax(1, qRound(newW / qMax(8.0, cellW))), item->widthUnits);
        }
    } else {
        const double newH = qMax(8.0, local.y() - m_pressRect.top());
        const double cellH = m_pressRect.height() / qMax(1, item->rowSpan);
        m_session.resizeItem(item->id, qMax(1, qRound(newH / qMax(8.0, cellH))), item->colSpan,
                             item->widthUnits);
    }
}

void LayoutEditorCanvas::showItemMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    auto* dup = menu.addAction(QStringLiteral("Duplicate"));
    auto* del = menu.addAction(QStringLiteral("Delete"));
    menu.addSeparator();
    auto* toFree = menu.addAction(QStringLiteral("Convert to free item"));
    auto* toCell = menu.addAction(QStringLiteral("Convert to cell"));
    menu.addSeparator();
    auto* raise = menu.addAction(QStringLiteral("Bring forward"));
    auto* lower = menu.addAction(QStringLiteral("Send backward"));
    const bool has = m_session.selection().target == EditorTarget::Item;
    for (QAction* a : {dup, del, toFree, toCell, raise, lower}) {
        a->setEnabled(has);
    }
    QAction* chosen = menu.exec(globalPos);
    if (chosen == dup) {
        m_session.duplicateSelected();
    } else if (chosen == del) {
        m_session.deleteSelected();
    } else if (chosen == toFree) {
        m_session.convertSelectedToFree();
    } else if (chosen == toCell) {
        m_session.convertSelectedToCell();
    } else if (chosen == raise) {
        m_session.raiseSelected();
    } else if (chosen == lower) {
        m_session.lowerSelected();
    }
}

void LayoutEditorCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    if (!id.isEmpty() && !m_session.isItemSelected(id)) {
        m_session.selectItem(id);
    }
    showItemMenu(event->globalPos());
}

void LayoutEditorCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_session.setPlaceKind(std::nullopt);
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        m_session.deleteSelected();
        return;
    }
    int dRow = 0;
    int dCol = 0;
    if (event->key() == Qt::Key_Left) {
        dCol = -1;
    } else if (event->key() == Qt::Key_Right) {
        dCol = 1;
    } else if (event->key() == Qt::Key_Up) {
        dRow = -1;
    } else if (event->key() == Qt::Key_Down) {
        dRow = 1;
    }
    if (dRow || dCol) {
        const int px = (event->modifiers() & Qt::ShiftModifier) ? 16 : 4;
        m_session.nudgeSelected(dRow, dCol, px);
        return;
    }
    QWidget::keyPressEvent(event);
}

void LayoutEditorCanvas::leaveEvent(QEvent*)
{
    if (!m_hoverId.isEmpty()) {
        m_hoverId.clear();
        update();
    }
}

} // namespace gazer
