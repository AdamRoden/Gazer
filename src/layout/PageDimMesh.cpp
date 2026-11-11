#include "layout/PageDim.h"

#include <QtGlobal>

namespace gazer {
namespace PageDimParse {

namespace {

QVector<PageTrackSize> padTracks(const QVector<PageTrackSize>& authored, int count)
{
    QVector<PageTrackSize> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.push_back(i < authored.size() ? authored[i] : PageTrackSize::starWeight(1.0));
    }
    return out;
}

QVector<double> resolveTracks(const QVector<PageTrackSize>& tracks, double inner, double axisRef,
                              double heightRef, const QSizeF& screen, bool keepFixed)
{
    const int count = tracks.size();
    QVector<double> sizes(count, 0.0);
    if (count <= 0) {
        return sizes;
    }
    const double sw = screen.width() > 0.0 ? screen.width() : axisRef;
    const double sh = screen.height() > 0.0 ? screen.height() : heightRef;
    double fixed = 0.0;
    double starTotal = 0.0;
    for (int i = 0; i < count; ++i) {
        if (tracks[i].isStar()) {
            starTotal += tracks[i].star > 0.0 ? tracks[i].star : 1.0;
            continue;
        }
        const double px = qMax(0.0, tracks[i].dim.resolve(axisRef, heightRef, sw, sh));
        sizes[i] = px;
        fixed += px;
    }
    double leftover = inner - fixed;
    if (leftover < 0.0 && fixed > 0.0 && !keepFixed) {
        const double scale = inner / fixed;
        for (int i = 0; i < count; ++i) {
            if (!tracks[i].isStar()) {
                sizes[i] *= scale;
            }
        }
        leftover = 0.0;
        starTotal = 0.0;
    } else if (leftover < 0.0 && keepFixed) {
        leftover = 0.0;
        starTotal = 0.0;
    }
    if (starTotal > 0.0 && leftover > 0.0) {
        for (int i = 0; i < count; ++i) {
            if (tracks[i].isStar()) {
                const double w = tracks[i].star > 0.0 ? tracks[i].star : 1.0;
                sizes[i] = leftover * (w / starTotal);
            }
        }
    }
    return sizes;
}

struct Mesh {
    QVector<double> colSizes;
    QVector<double> rowSizes;
    double originX = 0.0;
    double originY = 0.0;
    int gap = 0;

    [[nodiscard]] bool valid() const { return !colSizes.isEmpty() && !rowSizes.isEmpty(); }
};

double trackPixels(const PageTrackSize& track, const QSizeF& screen)
{
    if (track.isStar() || !track.dim.isSet()) {
        return 0.0;
    }
    const double sw = screen.width() > 0.0 ? screen.width() : 0.0;
    const double sh = screen.height() > 0.0 ? screen.height() : 0.0;
    const double axis = sh > 0.0 ? sh : 1.0;
    return qMax(0.0, track.dim.resolve(axis, axis, sw, sh));
}

double naturalGridHeight(const PageGrid& grid, const QSizeF& screen);

// A star row in a scrolling grid is the subgrid placed there. Cells have no
// intrinsic height, and a rowSpan other than 1 is left to the slot.
double starRowNatural(const PageGrid& grid, int row, const QSizeF& screen)
{
    double h = 0.0;
    for (const PageGrid& sub : grid.subGrids) {
        if (sub.row != row || sub.rowSpan != 1) {
            continue;
        }
        h = qMax(h, naturalGridHeight(sub, screen));
    }
    return h;
}

double naturalGridHeight(const PageGrid& grid, const QSizeF& screen)
{
    const int rows = qMax(1, grid.rows);
    const int gap = qMax(0, grid.gapPx);
    const int margin = qMax(0, grid.marginPx);
    const QVector<PageTrackSize> tracks = padTracks(grid.rowTracks, rows);
    double sum = 0.0;
    for (int r = 0; r < rows; ++r) {
        sum += tracks[r].isStar() ? starRowNatural(grid, r, screen) : trackPixels(tracks[r], screen);
    }
    return margin * 2.0 + gap * (rows - 1) + sum;
}

// Star rows become fixed pixel tracks of that natural height, then share the
// same keep-fixed pass as authored pixel rows. They do not take leftover
// viewport space, and they do not collapse when the fixed rows already fill it.
QVector<double> scrollRowSizes(const PageGrid& grid, double inner, const QSizeF& screen)
{
    const int rows = qMax(1, grid.rows);
    QVector<PageTrackSize> tracks = padTracks(grid.rowTracks, rows);
    for (int r = 0; r < rows; ++r) {
        if (!tracks[r].isStar()) {
            continue;
        }
        tracks[r] = PageTrackSize::fromDim(PageDim::pixels(starRowNatural(grid, r, screen)));
    }
    return resolveTracks(tracks, inner, inner, inner, screen, true);
}

double scrollContentHeight(const PageGrid& grid, const QRectF& slot, const QSizeF& screen)
{
    const int rows = qMax(1, grid.rows);
    const int gap = qMax(0, grid.gapPx);
    const int margin = qMax(0, grid.marginPx);
    if (slot.height() <= 0.0) {
        return 0.0;
    }
    const double inner = slot.height() - 2.0 * margin - gap * (rows - 1);
    if (inner <= 0.0) {
        return slot.height();
    }
    const QSizeF metrics(screen.width() > 0.0 ? screen.width() : slot.width(),
                         screen.height() > 0.0 ? screen.height() : slot.height());
    const QVector<double> sizes = scrollRowSizes(grid, inner, metrics);
    double sum = 0.0;
    for (double s : sizes) {
        sum += s;
    }
    return 2.0 * margin + gap * (rows - 1) + sum;
}

Mesh gridMesh(const PageGrid& grid, const QRectF& gridRect, const QSizeF& screen)
{
    const int cols = qMax(1, grid.columns);
    const int rows = qMax(1, grid.rows);
    const int gap = qMax(0, grid.gapPx);
    const int margin = qMax(0, grid.marginPx);
    const double innerW = gridRect.width() - 2.0 * margin - gap * (cols - 1);
    const double innerH = gridRect.height() - 2.0 * margin - gap * (rows - 1);
    Mesh m;
    if (innerW <= 0.0 || innerH <= 0.0) {
        return m;
    }
    const QSizeF metrics(screen.width() > 0.0 ? screen.width() : gridRect.width(),
                         screen.height() > 0.0 ? screen.height() : gridRect.height());
    m.gap = gap;
    m.originX = gridRect.left() + margin;
    m.originY = gridRect.top() + margin;
    m.colSizes =
        resolveTracks(padTracks(grid.columnTracks, cols), innerW, innerW, innerH, metrics, grid.scroll);
    if (grid.scroll) {
        m.rowSizes = scrollRowSizes(grid, innerH, metrics);
        const double total = scrollContentHeight(grid, gridRect, metrics);
        const int maxOff = qMax(0, qRound(total) - qRound(gridRect.height()));
        m.originY -= qBound(0, grid.scrollOffset, maxOff);
    } else {
        m.rowSizes = resolveTracks(padTracks(grid.rowTracks, rows), innerH, innerH, innerH, metrics,
                                   false);
    }
    return m;
}

double spanStart(const QVector<double>& sizes, int gap, int index)
{
    double v = 0.0;
    const int n = qBound(0, index, sizes.size());
    for (int i = 0; i < n; ++i) {
        v += sizes[i] + gap;
    }
    return v;
}

double spanLength(const QVector<double>& sizes, int gap, int index, int span)
{
    double v = 0.0;
    const int begin = qBound(0, index, sizes.size());
    const int end = qBound(begin, begin + qMax(1, span), sizes.size());
    for (int i = begin; i < end; ++i) {
        if (i > begin) {
            v += gap;
        }
        v += sizes[i];
    }
    return v;
}

int trackIndexAt(const QVector<double>& sizes, int gap, double local)
{
    if (sizes.isEmpty()) {
        return 0;
    }
    double y = 0.0;
    for (int i = 0; i < sizes.size(); ++i) {
        const double next = y + sizes[i] + (i + 1 < sizes.size() ? gap : 0);
        if (local < next || i == sizes.size() - 1) {
            return i;
        }
        y = next;
    }
    return sizes.size() - 1;
}

[[nodiscard]] bool isScrollChromeRole(const QString& role)
{
    return role.compare(QLatin1String("scrollbar"), Qt::CaseInsensitive) == 0
           || role.compare(QLatin1String("scrollUp"), Qt::CaseInsensitive) == 0
           || role.compare(QLatin1String("scrollDown"), Qt::CaseInsensitive) == 0;
}

[[nodiscard]] bool containsScrollChrome(const PageGrid& grid)
{
    for (const PageCell& cell : grid.cells) {
        if (isScrollChromeRole(cell.role)) {
            return true;
        }
    }
    for (const PageGrid& sub : grid.subGrids) {
        if (containsScrollChrome(sub)) {
            return true;
        }
    }
    return false;
}

QString formatWeight(double v)
{
    const qint64 whole = qRound64(v);
    if (qAbs(v - double(whole)) < 1e-6) {
        return QString::number(whole);
    }
    return QString::number(v, 'g', 12);
}

} // namespace

std::optional<StarBand> starBandBeforeRow(const PageGrid& grid, int row)
{
    const int rows = qMax(1, grid.rows);
    if (row < 0 || row >= rows) {
        return std::nullopt;
    }
    const QVector<PageTrackSize> tracks = padTracks(grid.rowTracks, rows);
    double before = 0.0;
    double total = 0.0;
    for (int i = 0; i < rows; ++i) {
        if (!tracks[i].isStar()) {
            return std::nullopt;
        }
        total += tracks[i].star;
        if (i < row) {
            before += tracks[i].star;
        }
    }
    if (total <= 0.0) {
        return std::nullopt;
    }
    const int margin = qMax(0, grid.marginPx);
    const int gap = qMax(0, grid.gapPx);
    const int trim = 2 * margin + gap * (rows - 1);
    const int pad = margin + gap * row;
    StarBand band;
    band.partsBefore = before;
    band.parts = total;
    band.topExpr = QStringLiteral("(A_ScreenHeight-%1)*%2/%3+%4")
                       .arg(trim)
                       .arg(formatWeight(before))
                       .arg(formatWeight(total))
                       .arg(pad);
    return band;
}

ScrollMetrics scrollMetrics(const PageGrid& grid, const QRectF& slot, const QSizeF& screen)
{
    ScrollMetrics m;
    m.visible = qMax(0, qRound(slot.height()));
    const QSizeF metrics(screen.width() > 0.0 ? screen.width() : slot.width(),
                         screen.height() > 0.0 ? screen.height() : slot.height());
    m.total = qMax(0, qRound(scrollContentHeight(grid, slot, metrics)));
    m.offset = qBound(0, grid.scrollOffset, qMax(0, m.total - m.visible));
    return m;
}

QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col, int rowSpan,
                int colSpan, const QSizeF& screen)
{
    const Mesh mesh = gridMesh(grid, gridRect, screen);
    if (!mesh.valid()) {
        return {};
    }
    return QRectF(mesh.originX + spanStart(mesh.colSizes, mesh.gap, col),
                  mesh.originY + spanStart(mesh.rowSizes, mesh.gap, row),
                  spanLength(mesh.colSizes, mesh.gap, col, colSpan),
                  spanLength(mesh.rowSizes, mesh.gap, row, rowSpan));
}

QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos,
                   const QSizeF& screen)
{
    if (!gridRect.contains(pos)) {
        return {-1, -1};
    }
    const Mesh mesh = gridMesh(grid, gridRect, screen);
    if (!mesh.valid()) {
        return {-1, -1};
    }
    return {trackIndexAt(mesh.colSizes, mesh.gap, pos.x() - mesh.originX),
            trackIndexAt(mesh.rowSizes, mesh.gap, pos.y() - mesh.originY)};
}

std::optional<ScrollFit> scrollFit(const PageGrid& grid, const QRectF& slot, const QSizeF& screen)
{
    if (grid.scroll) {
        return std::nullopt;
    }
    const PageGrid* scrolling = nullptr;
    for (const PageGrid& sub : grid.subGrids) {
        if (sub.scroll) {
            scrolling = &sub;
            break;
        }
    }
    if (!scrolling) {
        return std::nullopt;
    }

    ScrollFit fit;
    QVector<PageTrackSize> columns = grid.columnTracks;
    while (columns.size() < qMax(1, grid.columns)) {
        columns.push_back(PageTrackSize::starWeight(1.0));
    }
    for (int i = 0; i < grid.subGrids.size(); ++i) {
        const PageGrid& sub = grid.subGrids[i];
        if (sub.scroll || !containsScrollChrome(sub)) {
            continue;
        }
        fit.chrome.push_back(i);
        const int end = qMin(columns.size(), sub.col + qMax(1, sub.colSpan));
        for (int col = qMax(0, sub.col); col < end; ++col) {
            columns[col] = PageTrackSize::fromDim(PageDim::pixels(0));
        }
    }
    if (fit.chrome.isEmpty()) {
        return std::nullopt;
    }

    const QRectF childSlot = cellRect(grid, slot, scrolling->row, scrolling->col, scrolling->rowSpan,
                                      scrolling->colSpan, screen);
    fit.metrics = scrollMetrics(*scrolling, childSlot, screen);
    if (fit.metrics.total <= fit.metrics.visible) {
        fit.columns = std::move(columns);
    }
    return fit;
}

} // namespace PageDimParse
} // namespace gazer
