#include "editor/LayoutEditorIcons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace gazer {
namespace {

void stroke(QPainter& p, const QColor& c, qreal w = 1.8)
{
    QPen pen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
}

void fill(QPainter& p, const QColor& c)
{
    p.setPen(Qt::NoPen);
    p.setBrush(c);
}

void drawGlyph(QPainter& p, EditorGlyph g, const QColor& fg, const QColor& ac)
{
    switch (g) {
    case EditorGlyph::FileNew: {
        QPainterPath path;
        path.moveTo(7, 4);
        path.lineTo(14, 4);
        path.lineTo(18, 8);
        path.lineTo(18, 20);
        path.lineTo(7, 20);
        path.closeSubpath();
        stroke(p, fg);
        p.drawPath(path);
        p.drawLine(QPointF(14, 4), QPointF(14, 8));
        p.drawLine(QPointF(14, 8), QPointF(18, 8));
        break;
    }
    case EditorGlyph::FileOpen: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(4, 9, 16, 11), 2, 2);
        QPainterPath tab;
        tab.moveTo(4, 11);
        tab.lineTo(4, 7);
        tab.lineTo(10, 7);
        tab.lineTo(12, 9.5);
        tab.lineTo(20, 9.5);
        p.drawPath(tab);
        break;
    }
    case EditorGlyph::FileSave: {
        stroke(p, fg);
        p.drawLine(QPointF(12, 4), QPointF(12, 14));
        p.drawLine(QPointF(8, 10), QPointF(12, 14));
        p.drawLine(QPointF(16, 10), QPointF(12, 14));
        p.drawLine(QPointF(5, 18), QPointF(19, 18));
        p.drawLine(QPointF(5, 18), QPointF(5, 16));
        p.drawLine(QPointF(19, 18), QPointF(19, 16));
        break;
    }
    case EditorGlyph::Undo: {
        stroke(p, fg);
        QPainterPath path;
        path.moveTo(18, 16);
        path.arcTo(QRectF(6, 6, 12, 12), -20, 220);
        p.drawPath(path);
        p.drawLine(QPointF(6.5, 8), QPointF(6.5, 13));
        p.drawLine(QPointF(6.5, 8), QPointF(11, 8));
        break;
    }
    case EditorGlyph::Redo: {
        stroke(p, fg);
        QPainterPath path;
        path.moveTo(6, 16);
        path.arcTo(QRectF(6, 6, 12, 12), 200, -220);
        p.drawPath(path);
        p.drawLine(QPointF(17.5, 8), QPointF(17.5, 13));
        p.drawLine(QPointF(17.5, 8), QPointF(13, 8));
        break;
    }
    case EditorGlyph::Cut: {
        stroke(p, fg);
        p.drawEllipse(QRectF(5, 14, 5, 5));
        p.drawEllipse(QRectF(14, 14, 5, 5));
        p.drawLine(QPointF(7.5, 15), QPointF(16, 5));
        p.drawLine(QPointF(16.5, 15), QPointF(8, 5));
        break;
    }
    case EditorGlyph::Copy: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(8, 6, 10, 12), 1.5, 1.5);
        p.drawRoundedRect(QRectF(5, 9, 10, 12), 1.5, 1.5);
        break;
    }
    case EditorGlyph::Paste: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(6, 7, 12, 13), 2, 2);
        p.drawRoundedRect(QRectF(9, 4, 6, 5), 1.2, 1.2);
        break;
    }
    case EditorGlyph::Delete: {
        stroke(p, fg);
        p.drawLine(QPointF(8, 8), QPointF(16, 8));
        p.drawLine(QPointF(10, 8), QPointF(10, 6));
        p.drawLine(QPointF(14, 8), QPointF(14, 6));
        p.drawLine(QPointF(10, 6), QPointF(14, 6));
        p.drawRoundedRect(QRectF(8, 8, 8, 11), 1.4, 1.4);
        p.drawLine(QPointF(11, 11), QPointF(11, 16));
        p.drawLine(QPointF(13, 11), QPointF(13, 16));
        break;
    }
    case EditorGlyph::TestLive: {
        QPainterPath tri;
        tri.moveTo(8, 5);
        tri.lineTo(19, 12);
        tri.lineTo(8, 19);
        tri.closeSubpath();
        fill(p, ac);
        p.drawPath(tri);
        break;
    }
    case EditorGlyph::TestCanvas: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(4, 5, 16, 14), 2.5, 2.5);
        QPainterPath tri;
        tri.moveTo(10, 9);
        tri.lineTo(16, 12);
        tri.lineTo(10, 15);
        tri.closeSubpath();
        fill(p, ac);
        p.drawPath(tri);
        break;
    }
    case EditorGlyph::Fit: {
        stroke(p, fg, 1.9);
        p.drawLine(QPointF(5, 9), QPointF(5, 5));
        p.drawLine(QPointF(5, 5), QPointF(9, 5));
        p.drawLine(QPointF(15, 5), QPointF(19, 5));
        p.drawLine(QPointF(19, 5), QPointF(19, 9));
        p.drawLine(QPointF(19, 15), QPointF(19, 19));
        p.drawLine(QPointF(19, 19), QPointF(15, 19));
        p.drawLine(QPointF(9, 19), QPointF(5, 19));
        p.drawLine(QPointF(5, 19), QPointF(5, 15));
        break;
    }
    case EditorGlyph::Grid:
    case EditorGlyph::GridAdd: {
        stroke(p, fg, 1.6);
        p.drawRoundedRect(QRectF(5, 5, 14, 14), 1.5, 1.5);
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
        break;
    }
    case EditorGlyph::Button: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(4, 8, 16, 8), 3, 3);
        break;
    }
    case EditorGlyph::Label: {
        stroke(p, fg, 1.9);
        p.drawLine(QPointF(7, 6), QPointF(7, 18));
        p.drawLine(QPointF(7, 6), QPointF(12, 6));
        p.drawLine(QPointF(7, 12), QPointF(11, 12));
        p.drawLine(QPointF(14, 10), QPointF(19, 10));
        p.drawLine(QPointF(14, 14), QPointF(18, 14));
        break;
    }
    case EditorGlyph::Toggle: {
        fill(p, ac);
        p.drawRoundedRect(QRectF(4, 8, 16, 8), 4, 4);
        fill(p, fg);
        p.drawEllipse(QRectF(12.5, 9.2, 5.6, 5.6));
        break;
    }
    case EditorGlyph::Tab: {
        stroke(p, fg);
        QPainterPath path;
        path.moveTo(4, 18);
        path.lineTo(4, 9);
        path.lineTo(7, 9);
        path.lineTo(9, 6);
        path.lineTo(15, 6);
        path.lineTo(17, 9);
        path.lineTo(20, 9);
        path.lineTo(20, 18);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case EditorGlyph::Slider: {
        stroke(p, fg);
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        fill(p, ac);
        p.drawEllipse(QRectF(10, 8, 8, 8));
        break;
    }
    case EditorGlyph::Zone: {
        QPen pen(fg, 1.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
        break;
    }
    case EditorGlyph::SubGrid: {
        stroke(p, fg, 1.6);
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawRoundedRect(QRectF(8, 8, 11, 11), 1.2, 1.2);
        p.drawLine(QPointF(13.5, 8), QPointF(13.5, 19));
        p.drawLine(QPointF(8, 13.5), QPointF(19, 13.5));
        break;
    }
    case EditorGlyph::Style: {
        fill(p, ac);
        p.drawEllipse(QRectF(6, 6, 8, 8));
        fill(p, fg);
        p.drawEllipse(QRectF(12, 9, 7, 7));
        stroke(p, fg, 1.4);
        p.drawEllipse(QRectF(8, 13, 6, 6));
        break;
    }
    case EditorGlyph::Dwell: {
        stroke(p, fg, 1.6);
        p.drawEllipse(QRectF(5, 5, 14, 14));
        p.drawEllipse(QRectF(8, 8, 8, 8));
        fill(p, ac);
        p.drawEllipse(QRectF(11, 11, 2.4, 2.4));
        break;
    }
    case EditorGlyph::Duplicate: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(7, 5, 11, 11), 1.6, 1.6);
        p.drawRoundedRect(QRectF(5, 9, 11, 11), 1.6, 1.6);
        break;
    }
    case EditorGlyph::Page: {
        stroke(p, fg);
        p.drawRoundedRect(QRectF(7, 4, 10, 16), 1.6, 1.6);
        p.drawLine(QPointF(10, 9), QPointF(14, 9));
        p.drawLine(QPointF(10, 12), QPointF(16, 12));
        p.drawLine(QPointF(10, 15), QPointF(15, 15));
        break;
    }
    case EditorGlyph::ZoomIn: {
        stroke(p, fg, 1.8);
        p.drawLine(QPointF(7, 12), QPointF(17, 12));
        p.drawLine(QPointF(12, 7), QPointF(12, 17));
        break;
    }
    case EditorGlyph::ZoomOut: {
        stroke(p, fg, 1.8);
        p.drawLine(QPointF(7, 12), QPointF(17, 12));
        break;
    }
    case EditorGlyph::FitScreen: {
        stroke(p, fg, 1.7);
        p.drawRoundedRect(QRectF(4, 6, 16, 12), 2, 2);
        p.drawLine(QPointF(9, 20), QPointF(15, 20));
        break;
    }
    case EditorGlyph::Code: {
        stroke(p, fg, 1.8);
        p.drawLine(QPointF(8, 7), QPointF(4, 12));
        p.drawLine(QPointF(4, 12), QPointF(8, 17));
        p.drawLine(QPointF(16, 7), QPointF(20, 12));
        p.drawLine(QPointF(20, 12), QPointF(16, 17));
        break;
    }
    }
}

QPixmap renderGlyph(EditorGlyph g, const QColor& fg, const QColor& ac, int logical, qreal dpr)
{
    const int px = qMax(1, qRound(logical * dpr));
    QPixmap pm(px, px);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = qreal(logical) / 24.0;
    p.scale(s, s);
    drawGlyph(p, g, fg, ac);
    return pm;
}

} // namespace

QIcon editorGlyphIcon(EditorGlyph glyph, const ThemeColors& theme, int logicalPx)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QIcon icon;
    icon.addPixmap(renderGlyph(glyph, theme.text, theme.accent, logicalPx, dpr), QIcon::Normal);
    icon.addPixmap(renderGlyph(glyph, theme.textSecondary, theme.border, logicalPx, dpr),
                   QIcon::Disabled);
    if (dpr < 1.5) {
        icon.addPixmap(renderGlyph(glyph, theme.text, theme.accent, logicalPx, 2.0), QIcon::Normal);
    }
    return icon;
}

EditorGlyph glyphForItemKind(EditorItemKind kind)
{
    switch (kind) {
    case EditorItemKind::Label:
        return EditorGlyph::Label;
    case EditorItemKind::Toggle:
        return EditorGlyph::Toggle;
    case EditorItemKind::Tab:
        return EditorGlyph::Tab;
    case EditorItemKind::Slider:
        return EditorGlyph::Slider;
    case EditorItemKind::Zone:
        return EditorGlyph::Zone;
    case EditorItemKind::Button:
        break;
    }
    return EditorGlyph::Button;
}

EditorGlyph glyphForLeaf(const PageLeaf& leaf, bool zone)
{
    if (zone) {
        return EditorGlyph::Zone;
    }
    const QString role = leaf.role.trimmed().toLower();
    if (role == QLatin1String("label") || role == QLatin1String("value")
        || role == QLatin1String("display") || role == QLatin1String("preview")) {
        return EditorGlyph::Label;
    }
    if (role == QLatin1String("toggle") || role == QLatin1String("choice")) {
        return EditorGlyph::Toggle;
    }
    if (role == QLatin1String("tab")) {
        return EditorGlyph::Tab;
    }
    if (role == QLatin1String("slider")) {
        return EditorGlyph::Slider;
    }
    return EditorGlyph::Button;
}

} // namespace gazer
