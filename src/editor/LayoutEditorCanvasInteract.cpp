#include "editor/LayoutEditorCanvas.h"

#include "editor/LayoutEditorMenu.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>

namespace gazer {
namespace {

double snapThreshPx(double scale)
{
    return qBound(4.0, 8.0 / qMax(0.05, scale), 24.0);
}

double snapValue(double v, const QVector<double>& stops, double thresh, bool* hit = nullptr)
{
    double best = v;
    double bestD = thresh;
    for (double s : stops) {
        const double d = qAbs(v - s);
        if (d < bestD) {
            bestD = d;
            best = s;
        }
    }
    if (hit) {
        *hit = best != v;
    }
    return best;
}

void addRectStops(QVector<double>& xs, QVector<double>& ys, const QRectF& r, bool centers)
{
    xs.push_back(r.left());
    xs.push_back(r.right());
    ys.push_back(r.top());
    ys.push_back(r.bottom());
    if (centers) {
        xs.push_back(r.center().x());
        ys.push_back(r.center().y());
    }
}

struct CellSpan {
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
};

CellSpan spanFromEdges(const PageCell& cell, QPoint idx, Qt::Edges edges)
{
    CellSpan s{cell.row, cell.col, cell.rowSpan, cell.colSpan};
    const int row1 = s.row + s.rowSpan - 1;
    const int col1 = s.col + s.colSpan - 1;
    if (edges.testFlag(Qt::RightEdge)) {
        s.colSpan = qMax(1, idx.x() - s.col + 1);
    }
    if (edges.testFlag(Qt::LeftEdge)) {
        s.col = qBound(0, idx.x(), col1);
        s.colSpan = col1 - s.col + 1;
    }
    if (edges.testFlag(Qt::BottomEdge)) {
        s.rowSpan = qMax(1, idx.y() - s.row + 1);
    }
    if (edges.testFlag(Qt::TopEdge)) {
        s.row = qBound(0, idx.y(), row1);
        s.rowSpan = row1 - s.row + 1;
    }
    return s;
}

} // namespace

QString LayoutEditorCanvas::hitItem(const QPoint& pos, const ScreenMap& m) const
{
    for (int i = m.targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = m.targets[i];
        const QRectF r = targetRect(t);
        if (!r.isEmpty() && toCanvas(r, m).contains(pos)) {
            return t.id;
        }
        if (t.kind == PageTarget::Kind::Zone && !t.geom.dwellZone.isEmpty()
            && toCanvas(t.geom.dwellZone, m).contains(pos)) {
            return t.id;
        }
    }
    return {};
}

bool LayoutEditorCanvas::hitBoard(const QPoint& pos, const ScreenMap& m) const
{
    const QPointF virt = toVirt(pos, m);
    for (int i = m.grids.size() - 1; i >= 0; --i) {
        if (m.grids[i].visual.contains(virt)) {
            return true;
        }
    }
    return false;
}

QPoint LayoutEditorCanvas::cellAt(const QPoint& pos, const ScreenMap& m, QString* gridId) const
{
    const QPointF virt = toVirt(pos, m);
    for (int i = m.grids.size() - 1; i >= 0; --i) {
        const PageGridPaint& gp = m.grids[i];
        if (!gp.visual.contains(virt)) {
            continue;
        }
        const PageGrid* g = PageEdit::findGrid(m_session.document(), gp.gridId);
        if (!g) {
            continue;
        }
        if (gridId) {
            *gridId = gp.gridId;
        }
        const QPoint idx = PageHit::cellIndexAt(*g, gp.visual, virt);
        if (idx.x() < 0) {
            return {0, 0};
        }
        return idx;
    }
    if (gridId) {
        gridId->clear();
    }
    return {0, 0};
}

QRectF LayoutEditorCanvas::selectedRect(const ScreenMap& m) const
{
    const QString id = m_session.selection().itemId;
    if (id.isEmpty()) {
        return {};
    }
    const QRectF virt = boardItemRects(m).value(id);
    if (virt.isEmpty()) {
        return {};
    }
    return toCanvas(virt, m);
}

Qt::Edges LayoutEditorCanvas::hitHandle(const QPoint& pos, const ScreenMap& m) const
{
    const QRectF r = selectedRect(m);
    if (r.isEmpty()) {
        return {};
    }
    const double hs = 8.0;
    auto at = [&](const QPointF& c) {
        return QRectF(c.x() - hs, c.y() - hs, hs * 2.0, hs * 2.0).contains(pos);
    };
    if (at(r.topLeft())) {
        return Qt::TopEdge | Qt::LeftEdge;
    }
    if (at(r.topRight())) {
        return Qt::TopEdge | Qt::RightEdge;
    }
    if (at(r.bottomRight())) {
        return Qt::BottomEdge | Qt::RightEdge;
    }
    if (at(r.bottomLeft())) {
        return Qt::BottomEdge | Qt::LeftEdge;
    }
    if (at(QPointF(r.center().x(), r.top()))) {
        return Qt::TopEdge;
    }
    if (at(QPointF(r.right(), r.center().y()))) {
        return Qt::RightEdge;
    }
    if (at(QPointF(r.center().x(), r.bottom()))) {
        return Qt::BottomEdge;
    }
    if (at(QPointF(r.left(), r.center().y()))) {
        return Qt::LeftEdge;
    }
    return {};
}

void LayoutEditorCanvas::updateDragCursor(const ScreenMap& m, const QPoint& pos)
{
    const Qt::Edges e = m_drag == Drag::Resize ? m_resizeEdges : hitHandle(pos, m);
    const bool l = e.testFlag(Qt::LeftEdge);
    const bool r = e.testFlag(Qt::RightEdge);
    const bool t = e.testFlag(Qt::TopEdge);
    const bool b = e.testFlag(Qt::BottomEdge);
    if ((t && l) || (b && r)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((t && r) || (b && l)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (l || r) {
        setCursor(Qt::SizeHorCursor);
    } else if (t || b) {
        setCursor(Qt::SizeVerCursor);
    } else if (m_drag == Drag::Grid || m_drag == Drag::Pan || m_spaceHeld) {
        setCursor(Qt::SizeAllCursor);
    } else if (m_session.placeKind()) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void LayoutEditorCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    const ScreenMap m = map();
    m_pressPos = event->pos();
    m_dragPos = event->pos();
    m_drag = Drag::None;
    m_dragId.clear();

    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && m_spaceHeld)) {
        m_drag = Drag::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const Qt::Edges handle = hitHandle(event->pos(), m);
    if (handle) {
        m_drag = Drag::Resize;
        m_resizeEdges = handle;
        m_dragId = m_session.selection().itemId;
        m_pressRect = boardItemRects(m).value(m_dragId);
        return;
    }

    const QString id = hitItem(event->pos(), m);
    if (!id.isEmpty()) {
        m_drag = Drag::Move;
        m_dragId = id;
        m_pressRect = boardItemRects(m).value(id);
        m_session.selectItem(id, event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
        return;
    }

    if (m_session.placeKind() && *m_session.placeKind() == EditorItemKind::Zone) {
        m_session.addZoneAt(virtFromCanvas(event->pos(), m));
        return;
    }
    if (!hitBoard(event->pos(), m)) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    if (m_session.placeKind()) {
        QString gridId;
        const QPoint cell = cellAt(event->pos(), m, &gridId);
        m_session.addItemAt(*m_session.placeKind(), cell.y(), cell.x(), gridId);
        return;
    }
    QString gridId;
    (void)cellAt(event->pos(), m, &gridId);
    if (!gridId.isEmpty()) {
        m_session.selectGrid(gridId);
    }
    if (event->modifiers() & Qt::AltModifier) {
        m_drag = Drag::Grid;
        return;
    }
    m_drag = Drag::Rubber;
    m_rubber = QRect(event->pos(), QSize(1, 1));
}

void LayoutEditorCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const ScreenMap m = map();
    if (m_drag == Drag::Pan) {
        const QPoint d = event->pos() - m_pressPos;
        if (m.scaleX > 0.0) {
            m_lookAt -= QPointF(d.x() / m.scaleX, d.y() / m.scaleY);
        }
        m_pressPos = event->pos();
        update();
        return;
    }
    m_dragPos = event->pos();
    const QString hover = hitItem(event->pos(), m);
    if (hover != m_hoverId) {
        m_hoverId = hover;
        m_testProgress = 0.0;
    }
    if (m_drag == Drag::Rubber) {
        m_rubber = QRect(m_pressPos, event->pos()).normalized();
    }
    if (m_drag == Drag::Move || m_drag == Drag::Resize) {
        updateDragPreview(event->pos(), m);
    }
    updateDragCursor(m, event->pos());
    update();
}

void LayoutEditorCanvas::commitDrag(const QPoint& pos, const ScreenMap& m)
{
    switch (m_drag) {
    case Drag::Move:
        if (!m_dragId.isEmpty() && (pos - m_pressPos).manhattanLength() > 8) {
            if (PageEdit::isZone(m_session.document(), m_dragId)) {
                updateDragPreview(pos, m);
                QPointF delta = m_ghostVirt.isEmpty()
                                    ? QPointF((pos.x() - m_pressPos.x()) / qMax(0.001, m.scaleX),
                                              (pos.y() - m_pressPos.y()) / qMax(0.001, m.scaleY))
                                    : m_ghostVirt.topLeft() - m_pressRect.topLeft();
                const QString id = m_dragId;
                m_session.edit(QStringLiteral("Move zone"), [&](PageDocument& d) {
                    if (PageZone* z = PageEdit::findZone(d, id)) {
                        const double x0 = z->offset.x.isSet() ? z->offset.x.value : 0.0;
                        const double y0 = z->offset.y.isSet() ? z->offset.y.value : 0.0;
                        z->offset.x = PageDim::pixels(x0 + delta.x());
                        z->offset.y = PageDim::pixels(y0 + delta.y());
                    }
                });
            } else if (m.board.contains(pos)) {
                const QPoint cell = cellAt(pos, m);
                const PageCell* c = PageEdit::findCell(m_session.document(), m_dragId);
                const int dRow = c ? cell.y() - c->row : 0;
                const int dCol = c ? cell.x() - c->col : 0;
                if (m_session.selection().itemIds.size() > 1) {
                    m_session.moveSelected(dRow, dCol);
                } else {
                    m_session.moveItemToCell(m_dragId, cell.y(), cell.x());
                }
            }
        }
        break;
    case Drag::Resize:
        applyResize(pos, m);
        break;
    case Drag::Rubber: {
        const auto rects = boardItemRects(m);
        const QRectF band = QRectF(m_rubber.normalized());
        QStringList ids;
        for (auto it = rects.constBegin(); it != rects.constEnd(); ++it) {
            if (toCanvas(*it, m).intersects(band)) {
                ids.push_back(it.key());
            }
        }
        m_session.selectItems(ids);
        m_rubber = {};
        break;
    }
    case Drag::Grid: {
        const QPointF delta = pos - m_pressPos;
        QPoint topLeft = virtFromCanvas(m.board.topLeft().toPoint(), m);
        topLeft += QPoint(int(delta.x() / qMax(0.001, m.scaleX)),
                          int(delta.y() / qMax(0.001, m.scaleY)));
        QSize place = m.virtualScreen;
        QPoint origin(0, 0);
        const PageGrid* g = m_session.selectedGrid();
        if (g && g->desktopMode) {
            place = m.virtualDesktop.size();
            origin = m.virtualDesktop.topLeft();
        }
        m_session.snapWindowTo(topLeft - origin, place);
        break;
    }
    case Drag::Pan:
    case Drag::None:
        break;
    }
}

void LayoutEditorCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drag == Drag::Pan
        && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_drag = Drag::None;
        update();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }
    commitDrag(event->pos(), map());
    m_drag = Drag::None;
    m_resizeEdges = {};
    m_dragId.clear();
    m_dragPos = event->pos();
    clearDragPreview();
    update();
}

void LayoutEditorCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    const PageLeaf* item = id.isEmpty() ? nullptr : m_session.itemById(id);
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

void LayoutEditorCanvas::clearDragPreview()
{
    m_ghostVirt = {};
    m_guides.clear();
}

QRectF LayoutEditorCanvas::snapMoveDelta(QPointF* delta, const QRectF& virt, const ScreenMap& m)
{
    m_guides.clear();
    if (!delta || virt.isEmpty()) {
        return virt;
    }
    QVector<double> xs;
    QVector<double> ys;
    addRectStops(xs, ys, QRectF(QPointF(0, 0), QSizeF(m.virtualScreen)), true);
    addRectStops(xs, ys, QRectF(m.virtualDesktop), false);
    for (const PageGridPaint& g : m.grids) {
        addRectStops(xs, ys, g.visual, true);
    }
    for (const PageTarget& t : m.targets) {
        if (t.id == m_dragId) {
            continue;
        }
        const QRectF r = targetRect(t);
        if (!r.isEmpty()) {
            addRectStops(xs, ys, r, true);
        }
    }
    const double thresh = snapThreshPx(m.scaleX);
    QRectF moved = virt.translated(*delta);
    auto bestAdj = [&](double a, double b, double c, const QVector<double>& stops, double* hit) {
        double adj = 0;
        double best = thresh;
        bool any = false;
        double h = 0;
        for (double edge : {a, b, c}) {
            bool ok = false;
            const double s = snapValue(edge, stops, thresh, &ok);
            if (!ok) {
                continue;
            }
            const double d = s - edge;
            if (qAbs(d) < best) {
                best = qAbs(d);
                adj = d;
                h = s;
                any = true;
            }
        }
        if (hit && any) {
            *hit = h;
        }
        return any ? adj : 0.0;
    };
    double hitX = 0;
    double hitY = 0;
    const double dx = bestAdj(moved.left(), moved.center().x(), moved.right(), xs, &hitX);
    const double dy = bestAdj(moved.top(), moved.center().y(), moved.bottom(), ys, &hitY);
    delta->rx() += dx;
    delta->ry() += dy;
    moved = virt.translated(*delta);
    if (dx != 0.0) {
        m_guides.push_back(QLineF(hitX, 0, hitX, m.virtualScreen.height()));
    }
    if (dy != 0.0) {
        m_guides.push_back(QLineF(0, hitY, m.virtualScreen.width(), hitY));
    }
    return moved;
}

QRectF LayoutEditorCanvas::snapRect(const QRectF& virt, Qt::Edges edges, const ScreenMap& m)
{
    m_guides.clear();
    QVector<double> xs;
    QVector<double> ys;
    addRectStops(xs, ys, QRectF(QPointF(0, 0), QSizeF(m.virtualScreen)), false);
    for (const PageTarget& t : m.targets) {
        if (t.id == m_dragId) {
            continue;
        }
        const QRectF r = targetRect(t);
        if (!r.isEmpty()) {
            addRectStops(xs, ys, r, false);
        }
    }
    const double thresh = snapThreshPx(m.scaleX);
    QRectF out = virt;
    auto snapEdge = [&](double v, const QVector<double>& stops, bool vertical) {
        bool ok = false;
        const double s = snapValue(v, stops, thresh, &ok);
        if (ok) {
            if (vertical) {
                m_guides.push_back(QLineF(s, 0, s, m.virtualScreen.height()));
            } else {
                m_guides.push_back(QLineF(0, s, m.virtualScreen.width(), s));
            }
            return s;
        }
        return v;
    };
    if (edges.testFlag(Qt::LeftEdge)) {
        out.setLeft(snapEdge(out.left(), xs, true));
    }
    if (edges.testFlag(Qt::RightEdge)) {
        out.setRight(snapEdge(out.right(), xs, true));
    }
    if (edges.testFlag(Qt::TopEdge)) {
        out.setTop(snapEdge(out.top(), ys, false));
    }
    if (edges.testFlag(Qt::BottomEdge)) {
        out.setBottom(snapEdge(out.bottom(), ys, false));
    }
    if (out.width() < 16.0) {
        if (edges.testFlag(Qt::LeftEdge)) {
            out.setLeft(out.right() - 16.0);
        } else {
            out.setWidth(16.0);
        }
    }
    if (out.height() < 16.0) {
        if (edges.testFlag(Qt::TopEdge)) {
            out.setTop(out.bottom() - 16.0);
        } else {
            out.setHeight(16.0);
        }
    }
    return out;
}

void LayoutEditorCanvas::updateDragPreview(const QPoint& pos, const ScreenMap& m)
{
    m_ghostVirt = {};
    m_guides.clear();
    if (m_drag == Drag::Move && PageEdit::isZone(m_session.document(), m_dragId)
        && !m_pressRect.isEmpty()) {
        QPointF delta((pos.x() - m_pressPos.x()) / qMax(0.001, m.scaleX),
                      (pos.y() - m_pressPos.y()) / qMax(0.001, m.scaleY));
        m_ghostVirt = snapMoveDelta(&delta, m_pressRect, m);
        return;
    }
    if (m_drag == Drag::Move) {
        QString gridId;
        const QPoint idx = cellAt(pos, m, &gridId);
        const PageGrid* g = PageEdit::findGrid(m_session.document(), gridId);
        const QRectF gv = gridVisual(m, gridId);
        const PageCell* cell = PageEdit::findCell(m_session.document(), m_dragId);
        if (g && !gv.isEmpty() && cell && idx.x() >= 0) {
            m_ghostVirt = PageHit::cellRect(*g, gv, idx.y(), idx.x(), cell->rowSpan, cell->colSpan);
        }
        return;
    }
    if (m_drag != Drag::Resize || m_pressRect.isEmpty()) {
        return;
    }
    if (PageEdit::isZone(m_session.document(), m_dragId)) {
        const QPointF virt = toVirt(pos, m);
        QRectF r = m_pressRect;
        if (m_resizeEdges.testFlag(Qt::LeftEdge)) {
            r.setLeft(virt.x());
        }
        if (m_resizeEdges.testFlag(Qt::RightEdge)) {
            r.setRight(virt.x());
        }
        if (m_resizeEdges.testFlag(Qt::TopEdge)) {
            r.setTop(virt.y());
        }
        if (m_resizeEdges.testFlag(Qt::BottomEdge)) {
            r.setBottom(virt.y());
        }
        m_ghostVirt = snapRect(r.normalized(), m_resizeEdges, m);
        return;
    }
    const PageCell* cell = PageEdit::findCell(m_session.document(), m_dragId);
    if (!cell) {
        return;
    }
    QString gridId;
    const QPoint idx = cellAt(pos, m, &gridId);
    if (idx.x() < 0) {
        return;
    }
    const CellSpan span = spanFromEdges(*cell, idx, m_resizeEdges);
    const PageGrid* g = PageEdit::findGrid(m_session.document(), gridId);
    const QRectF gv = gridVisual(m, g ? g->id : gridId);
    if (g && !gv.isEmpty()) {
        m_ghostVirt = PageHit::cellRect(*g, gv, span.row, span.col, span.rowSpan, span.colSpan);
    }
}

void LayoutEditorCanvas::applyResize(const QPoint& pos, const ScreenMap& m)
{
    updateDragPreview(pos, m);
    if (PageEdit::isZone(m_session.document(), m_dragId)) {
        QRectF r = m_ghostVirt.isEmpty() ? m_pressRect : m_ghostVirt;
        if (r.width() < 1 || r.height() < 1) {
            return;
        }
        const QString id = m_dragId;
        const QRectF start = m_pressRect;
        m_session.edit(QStringLiteral("Resize %1").arg(id), [&](PageDocument& d) {
            if (PageZone* z = PageEdit::findZone(d, id)) {
                const double x0 = z->offset.x.isSet() ? z->offset.x.value : 0.0;
                const double y0 = z->offset.y.isSet() ? z->offset.y.value : 0.0;
                z->offset.x = PageDim::pixels(x0 + (r.left() - start.left()));
                z->offset.y = PageDim::pixels(y0 + (r.top() - start.top()));
                z->size.x = PageDim::pixels(r.width());
                z->size.y = PageDim::pixels(r.height());
            }
        });
        return;
    }
    const PageCell* cell = PageEdit::findCell(m_session.document(), m_dragId);
    if (!cell) {
        return;
    }
    QString gridId;
    const QPoint c = cellAt(pos, m, &gridId);
    if (c.x() < 0) {
        return;
    }
    const CellSpan span = spanFromEdges(*cell, c, m_resizeEdges);
    const QString id = m_dragId;
    m_session.edit(QStringLiteral("Resize %1").arg(id), [&](PageDocument& d) {
        if (PageCell* cellMut = PageEdit::findCell(d, id)) {
            cellMut->row = span.row;
            cellMut->col = span.col;
            cellMut->rowSpan = span.rowSpan;
            cellMut->colSpan = span.colSpan;
            if (PageGrid* g = PageEdit::gridOwningCell(d, id)) {
                PageEdit::expandForCell(*g, *cellMut);
            }
        }
    });
}

void LayoutEditorCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        m_spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
        return;
    }
    const int step = event->modifiers() & Qt::ShiftModifier ? 16 : 1;
    if (event->key() == Qt::Key_Left) {
        m_session.nudgeSelected(0, -1, step);
    } else if (event->key() == Qt::Key_Right) {
        m_session.nudgeSelected(0, 1, step);
    } else if (event->key() == Qt::Key_Up) {
        m_session.nudgeSelected(-1, 0, step);
    } else if (event->key() == Qt::Key_Down) {
        m_session.nudgeSelected(1, 0, step);
    } else if (event->key() == Qt::Key_Delete) {
        m_session.deleteSelected();
    } else if (event->key() == Qt::Key_Escape) {
        m_session.setPlaceKind(std::nullopt);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void LayoutEditorCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        m_spaceHeld = false;
        if (m_drag != Drag::Pan) {
            unsetCursor();
        }
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void LayoutEditorCanvas::leaveEvent(QEvent*)
{
    m_hoverId.clear();
    update();
}

void LayoutEditorCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const QString id = hitItem(event->pos(), map());
    if (!id.isEmpty() && !m_session.isItemSelected(id)) {
        m_session.selectItem(id);
    }
    execEditorItemMenu(this, event->globalPos(), m_session);
}

} // namespace gazer
