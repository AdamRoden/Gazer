#include "layout/PageDim.h"

#include <QStringList>
#include <QtGlobal>

namespace gazer {

namespace PageDimParse {

namespace {

QString norm(const QString& s)
{
    return s.trimmed().toLower();
}

[[nodiscard]] bool isNumericToken(const QString& t, double* out = nullptr)
{
    if (t.isEmpty()) {
        return false;
    }
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (!ok) {
        return false;
    }
    for (const QChar c : t) {
        if (c.isLetter() && c != QLatin1Char('e') && c != QLatin1Char('E')) {
            return false;
        }
    }
    if (out) {
        *out = v;
    }
    return true;
}

[[nodiscard]] bool isSimpleFraction(const QString& t, double* out)
{
    const int slash = t.indexOf(QLatin1Char('/'));
    if (slash <= 0 || t.indexOf(QLatin1Char('/'), slash + 1) >= 0) {
        return false;
    }
    const QString num = t.left(slash).trimmed();
    const QString den = t.mid(slash + 1).trimmed();
    if (!isNumericToken(num) || !isNumericToken(den)) {
        return false;
    }
    bool okNum = false;
    bool okDen = false;
    const double n = num.toDouble(&okNum);
    const double d = den.toDouble(&okDen);
    if (!okNum || !okDen || d == 0.0) {
        return false;
    }
    if (out) {
        *out = n / d;
    }
    return true;
}

struct ExprEval {
    QString s;
    int i = 0;
    QString* error = nullptr;
    double screenW = 0.0;
    double screenH = 0.0;
    bool failed = false;

    void fail(const QString& msg)
    {
        if (!failed) {
            failed = true;
            if (error) {
                *error = msg;
            }
        }
    }

    void skip()
    {
        while (i < s.size() && s.at(i).isSpace()) {
            ++i;
        }
    }

    bool eat(QChar c)
    {
        skip();
        if (i < s.size() && s.at(i) == c) {
            ++i;
            return true;
        }
        return false;
    }

    double parse()
    {
        const double v = parseAdd();
        skip();
        if (!failed && i < s.size()) {
            fail(QStringLiteral("Unexpected '%1' in '%2'").arg(s.mid(i), s));
            return 0.0;
        }
        return failed ? 0.0 : v;
    }

    double parseAdd()
    {
        double v = parseMul();
        for (;;) {
            if (failed) {
                return 0.0;
            }
            if (eat(QLatin1Char('+'))) {
                v += parseMul();
            } else if (eat(QLatin1Char('-'))) {
                v -= parseMul();
            } else {
                return v;
            }
        }
    }

    double parseMul()
    {
        double v = parseUnary();
        for (;;) {
            if (failed) {
                return 0.0;
            }
            if (eat(QLatin1Char('*'))) {
                v *= parseUnary();
            } else if (eat(QLatin1Char('/'))) {
                const double d = parseUnary();
                if (d == 0.0) {
                    fail(QStringLiteral("Division by zero in '%1'").arg(s));
                    return 0.0;
                }
                v /= d;
            } else {
                return v;
            }
        }
    }

    double parseUnary()
    {
        if (eat(QLatin1Char('+'))) {
            return parseUnary();
        }
        if (eat(QLatin1Char('-'))) {
            return -parseUnary();
        }
        return parsePrimary();
    }

    double parseClamp()
    {
        if (!eat(QLatin1Char('('))) {
            fail(QStringLiteral("Expected '(' after clamp in '%1'").arg(s));
            return 0.0;
        }
        const double v = parseAdd();
        if (failed) {
            return 0.0;
        }
        if (!eat(QLatin1Char(','))) {
            fail(QStringLiteral("clamp() takes 3 arguments (value, min, max) in '%1'").arg(s));
            return 0.0;
        }
        const double lo = parseAdd();
        if (failed) {
            return 0.0;
        }
        if (!eat(QLatin1Char(','))) {
            fail(QStringLiteral("clamp() takes 3 arguments (value, min, max) in '%1'").arg(s));
            return 0.0;
        }
        const double hi = parseAdd();
        if (failed) {
            return 0.0;
        }
        if (eat(QLatin1Char(','))) {
            fail(QStringLiteral("clamp() takes 3 arguments (value, min, max) in '%1'").arg(s));
            return 0.0;
        }
        if (!eat(QLatin1Char(')'))) {
            fail(QStringLiteral("Missing ')' in '%1'").arg(s));
            return 0.0;
        }
        // If the floor exceeds the cap, honor the cap so boards never overflow the screen.
        if (lo > hi) {
            return hi;
        }
        return qBound(lo, v, hi);
    }

    double parsePrimary()
    {
        skip();
        if (failed) {
            return 0.0;
        }
        if (i >= s.size()) {
            fail(QStringLiteral("Incomplete expression '%1'").arg(s));
            return 0.0;
        }
        if (eat(QLatin1Char('('))) {
            const double v = parseAdd();
            if (!eat(QLatin1Char(')'))) {
                fail(QStringLiteral("Missing ')' in '%1'").arg(s));
                return 0.0;
            }
            return v;
        }
        const QChar c = s.at(i);
        if (c.isLetter() || c == QLatin1Char('_')) {
            const int start = i;
            ++i;
            while (i < s.size()) {
                const QChar n = s.at(i);
                if (!n.isLetterOrNumber() && n != QLatin1Char('_')) {
                    break;
                }
                ++i;
            }
            const QString id = s.mid(start, i - start);
            const QString key = id.toLower();
            if (key == QLatin1String("clamp")) {
                return parseClamp();
            }
            if (key == QLatin1String("a_screenwidth")) {
                return screenW;
            }
            if (key == QLatin1String("a_screenheight")) {
                return screenH;
            }
            fail(QStringLiteral("Unknown identifier '%1'").arg(id));
            return 0.0;
        }
        if (c.isDigit() || c == QLatin1Char('.')) {
            const int start = i;
            bool seenDot = false;
            while (i < s.size()) {
                const QChar n = s.at(i);
                if (n.isDigit()) {
                    ++i;
                    continue;
                }
                if (n == QLatin1Char('.') && !seenDot) {
                    seenDot = true;
                    ++i;
                    continue;
                }
                if ((n == QLatin1Char('e') || n == QLatin1Char('E')) && i + 1 < s.size()) {
                    const QChar next = s.at(i + 1);
                    if (next.isDigit() || next == QLatin1Char('+') || next == QLatin1Char('-')) {
                        i += 2;
                        while (i < s.size() && s.at(i).isDigit()) {
                            ++i;
                        }
                        break;
                    }
                }
                break;
            }
            const QString num = s.mid(start, i - start);
            bool ok = false;
            const double v = num.toDouble(&ok);
            if (!ok) {
                fail(QStringLiteral("Invalid number '%1' in '%2'").arg(num, s));
                return 0.0;
            }
            return v;
        }
        fail(QStringLiteral("Unexpected '%1' in '%2'").arg(QString(c), s));
        return 0.0;
    }
};

[[nodiscard]] QStringList splitTopLevelCsv(const QString& s, QString* error)
{
    QStringList out;
    int depth = 0;
    int start = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(')')) {
            --depth;
            if (depth < 0) {
                if (error) {
                    *error = QStringLiteral("Unmatched ')' in '%1'").arg(s);
                }
                return {};
            }
        } else if (c == QLatin1Char(',') && depth == 0) {
            out.push_back(s.mid(start, i - start));
            start = i + 1;
        }
    }
    if (depth != 0) {
        if (error) {
            *error = QStringLiteral("Unmatched '(' in '%1'").arg(s);
        }
        return {};
    }
    out.push_back(s.mid(start));
    return out;
}

[[nodiscard]] bool splitTopLevelPair(const QString& s, QString* left, QString* right, QString* error)
{
    QString splitErr;
    const QStringList parts = splitTopLevelCsv(s, &splitErr);
    if (!splitErr.isEmpty()) {
        if (error) {
            *error = splitErr;
        }
        return false;
    }
    if (parts.size() != 2) {
        if (error) {
            *error = QStringLiteral("Expected x,y pair, got '%1'").arg(s);
        }
        return false;
    }
    if (left) {
        *left = parts[0];
    }
    if (right) {
        *right = parts[1];
    }
    return true;
}

} // namespace

double evalDimExpression(const QString& expr, double screenW, double screenH, QString* error)
{
    ExprEval e;
    e.s = expr.trimmed();
    e.error = error;
    e.screenW = screenW;
    e.screenH = screenH;
    if (e.s.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty expression");
        }
        return 0.0;
    }
    return e.parse();
}

bool looksLikeExpression(const QString& t)
{
    for (const QChar c : t) {
        if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('*')
            || c == QLatin1Char('+') || c == QLatin1Char('(') || c == QLatin1Char(')')) {
            return true;
        }
    }
    int slashes = 0;
    int minuses = 0;
    for (int i = 0; i < t.size(); ++i) {
        if (t.at(i) == QLatin1Char('/')) {
            ++slashes;
        } else if (t.at(i) == QLatin1Char('-') && i > 0) {
            ++minuses;
        }
    }
    return slashes > 1 || minuses > 0;
}

} // namespace PageDimParse

double PageDim::resolve(double axisRef, double heightRef, double screenWidth,
                        double screenHeight) const
{
    if (unit == Unit::Pixels) {
        return value;
    }
    if (unit == Unit::HeightProportion) {
        return heightRef * value;
    }
    if (unit == Unit::Proportion) {
        return axisRef * value;
    }
    if (unit == Unit::Expression) {
        return PageDimParse::evalDimExpression(expr, screenWidth, screenHeight, nullptr);
    }
    return 0.0;
}

namespace PageDimParse {

PageDim parse(const QString& token, QString* error)
{
    QString t = token.trimmed();
    if (t.isEmpty()) {
        return {};
    }

    bool heightRel = false;
    if (t.size() > 1 && (t.endsWith(QLatin1Char('h')) || t.endsWith(QLatin1Char('H')))) {
        const QString body = t.chopped(1).trimmed();
        if (isNumericToken(body) || isSimpleFraction(body, nullptr)) {
            heightRel = true;
            t = body;
        }
    }

    PageDim dim;
    double frac = 0.0;
    if (isSimpleFraction(t, &frac)) {
        dim = PageDim::proportion(frac);
    } else if (isNumericToken(t)) {
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Invalid dimension '%1'").arg(token);
            }
            return {};
        }
        dim = t.contains(QLatin1Char('.')) ? PageDim::proportion(v) : PageDim::pixels(v);
    } else if (looksLikeExpression(t) || t.contains(QLatin1Char('/'))) {
        if (heightRel) {
            if (error) {
                *error = QStringLiteral("Height suffix cannot apply to expression '%1'").arg(token);
            }
            return {};
        }
        QString err;
        (void)evalDimExpression(t, 1920.0, 1080.0, &err);
        if (!err.isEmpty()) {
            if (error) {
                *error = err;
            }
            return {};
        }
        dim = PageDim::expression(t);
    } else {
        if (error) {
            *error = QStringLiteral("Invalid dimension '%1'").arg(token);
        }
        return {};
    }
    if (heightRel) {
        if (dim.unit == PageDim::Unit::Pixels || dim.unit == PageDim::Unit::Expression) {
            if (error) {
                *error = QStringLiteral("Height suffix requires a proportion '%1'").arg(token);
            }
            return {};
        }
        dim.unit = PageDim::Unit::HeightProportion;
    }
    return dim;
}

PageDimPair parsePair(const QString& csv, QString* error)
{
    const QString s = csv.trimmed();
    if (s.isEmpty()) {
        return {};
    }
    QString left;
    QString right;
    if (!splitTopLevelPair(s, &left, &right, error)) {
        return {};
    }
    PageDimPair out;
    QString err;
    out.x = parse(left, &err);
    if (!err.isEmpty()) {
        if (error) {
            *error = err;
        }
        return {};
    }
    out.y = parse(right, &err);
    if (!err.isEmpty()) {
        if (error) {
            *error = err;
        }
        return {};
    }
    return out;
}

QVector<int> parseIntList(const QString& csv, QString* error)
{
    QVector<int> out;
    const QString s = csv.trimmed();
    if (s.isEmpty()) {
        return out;
    }
    const QStringList parts = s.split(QLatin1Char(','));
    for (const QString& p : parts) {
        const QString t = p.trimmed();
        if (t.isEmpty()) {
            continue;
        }
        bool ok = false;
        const int n = t.toInt(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Invalid integer '%1' in '%2'").arg(t, s);
            }
            return {};
        }
        out.push_back(n);
    }
    return out;
}

PageAnchor parseAnchor(const QString& name, bool* ok)
{
    const QString n = norm(name);
    if (ok) {
        *ok = true;
    }
    if (n.isEmpty() || n == QLatin1String("topleft") || n == QLatin1String("nw")
        || n == QLatin1String("northwest")) {
        return PageAnchor::TopLeft;
    }
    if (n == QLatin1String("top") || n == QLatin1String("topcenter")
        || n == QLatin1String("up") || n == QLatin1String("n") || n == QLatin1String("north")) {
        return PageAnchor::Top;
    }
    if (n == QLatin1String("topright") || n == QLatin1String("ne")
        || n == QLatin1String("northeast")) {
        return PageAnchor::TopRight;
    }
    if (n == QLatin1String("left") || n == QLatin1String("leftcenter")
        || n == QLatin1String("centerleft") || n == QLatin1String("w")
        || n == QLatin1String("west")) {
        return PageAnchor::Left;
    }
    if (n == QLatin1String("center")) {
        return PageAnchor::Center;
    }
    if (n == QLatin1String("right") || n == QLatin1String("rightcenter")
        || n == QLatin1String("centerright") || n == QLatin1String("e")
        || n == QLatin1String("east")) {
        return PageAnchor::Right;
    }
    if (n == QLatin1String("bottomleft") || n == QLatin1String("sw")
        || n == QLatin1String("southwest")) {
        return PageAnchor::BottomLeft;
    }
    if (n == QLatin1String("bottom") || n == QLatin1String("bottomcenter")
        || n == QLatin1String("down") || n == QLatin1String("s") || n == QLatin1String("south")) {
        return PageAnchor::Bottom;
    }
    if (n == QLatin1String("bottomright") || n == QLatin1String("se")
        || n == QLatin1String("southeast")) {
        return PageAnchor::BottomRight;
    }
    if (ok) {
        *ok = false;
    }
    return PageAnchor::TopLeft;
}

QString anchorName(PageAnchor a)
{
    switch (a) {
    case PageAnchor::TopLeft:
        return QStringLiteral("TopLeft");
    case PageAnchor::Top:
        return QStringLiteral("Top");
    case PageAnchor::TopRight:
        return QStringLiteral("TopRight");
    case PageAnchor::Left:
        return QStringLiteral("Left");
    case PageAnchor::Center:
        return QStringLiteral("Center");
    case PageAnchor::Right:
        return QStringLiteral("Right");
    case PageAnchor::BottomLeft:
        return QStringLiteral("BottomLeft");
    case PageAnchor::Bottom:
        return QStringLiteral("Bottom");
    case PageAnchor::BottomRight:
        return QStringLiteral("BottomRight");
    }
    return QStringLiteral("TopLeft");
}

QString token(const PageDim& d)
{
    if (!d.isSet()) {
        return {};
    }
    if (d.unit == PageDim::Unit::Expression) {
        return d.expr;
    }
    if (d.unit == PageDim::Unit::Proportion || d.unit == PageDim::Unit::HeightProportion) {
        QString t = QString::number(d.value, 'g', 8);
        if (d.unit == PageDim::Unit::HeightProportion) {
            t += QLatin1Char('h');
        }
        return t;
    }
    if (qFuzzyCompare(d.value, qRound(d.value))) {
        return QString::number(qRound(d.value));
    }
    return QString::number(d.value, 'g', 8);
}

PageTrackSize parseTrack(const QString& token, QString* error)
{
    const QString t = token.trimmed();
    if (t.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty track size");
        }
        return {};
    }
    if (t.size() > 2 && t.endsWith(QLatin1String("px"), Qt::CaseInsensitive)) {
        double px = 0.0;
        if (isNumericToken(t.chopped(2).trimmed(), &px)) {
            return PageTrackSize::fromDim(PageDim::pixels(px));
        }
    }
    if (t.endsWith(QLatin1Char('*'))) {
        const QString prefix = t.chopped(1).trimmed();
        if (prefix.isEmpty()) {
            return PageTrackSize::starWeight(1.0);
        }
        double star = 0.0;
        if (isNumericToken(prefix, &star) && star > 0.0) {
            return PageTrackSize::starWeight(star);
        }
        if (error) {
            *error = QStringLiteral("Invalid star track '%1'").arg(token);
        }
        return {};
    }
    QString err;
    const PageDim dim = parse(t, &err);
    if (!dim.isSet()) {
        if (error) {
            *error = err.isEmpty() ? QStringLiteral("Invalid track size '%1'").arg(token) : err;
        }
        return {};
    }
    return PageTrackSize::fromDim(dim);
}

QVector<PageTrackSize> parseTrackList(const QString& csv, QString* error)
{
    QVector<PageTrackSize> out;
    const QString s = csv.trimmed();
    if (s.isEmpty()) {
        return out;
    }
    QString splitErr;
    const QStringList parts = splitTopLevelCsv(s, &splitErr);
    if (!splitErr.isEmpty()) {
        if (error) {
            *error = splitErr;
        }
        return {};
    }
    for (const QString& part : parts) {
        if (part.trimmed().isEmpty()) {
            continue;
        }
        QString err;
        const PageTrackSize t = parseTrack(part, &err);
        if (!err.isEmpty() || (t.kind == PageTrackSize::Kind::Dim && !t.dim.isSet())) {
            if (error) {
                *error = err.isEmpty() ? QStringLiteral("Invalid track size '%1'").arg(part) : err;
            }
            return {};
        }
        out.push_back(t);
    }
    return out;
}

QString token(const PageTrackSize& t)
{
    if (t.kind == PageTrackSize::Kind::Star) {
        if (qAbs(t.star - 1.0) < 1e-9) {
            return QStringLiteral("*");
        }
        if (qFuzzyCompare(t.star, static_cast<double>(qRound(t.star)))) {
            return QString::number(qRound(t.star)) + QLatin1Char('*');
        }
        return QString::number(t.star, 'g', 8) + QLatin1Char('*');
    }
    return token(t.dim);
}

QString tokenList(const QVector<PageTrackSize>& tracks)
{
    QStringList parts;
    parts.reserve(tracks.size());
    for (const PageTrackSize& t : tracks) {
        parts.push_back(token(t));
    }
    return parts.join(QLatin1Char(','));
}

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
                              double heightRef, const QSizeF& screen)
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
    if (leftover < 0.0 && fixed > 0.0) {
        const double scale = inner / fixed;
        for (int i = 0; i < count; ++i) {
            if (!tracks[i].isStar()) {
                sizes[i] *= scale;
            }
        }
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
    m.colSizes = resolveTracks(padTracks(grid.columnTracks, cols), innerW, innerW, innerH, metrics);
    m.rowSizes = resolveTracks(padTracks(grid.rowTracks, rows), innerH, innerH, innerH, metrics);
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

} // namespace

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

QPoint anchorDelta(PageAnchor a, int amount)
{
    int dx = 0;
    int dy = 0;
    switch (a) {
    case PageAnchor::TopLeft:
        dx = -1;
        dy = -1;
        break;
    case PageAnchor::Top:
        dy = -1;
        break;
    case PageAnchor::TopRight:
        dx = 1;
        dy = -1;
        break;
    case PageAnchor::Left:
        dx = -1;
        break;
    case PageAnchor::Center:
        break;
    case PageAnchor::Right:
        dx = 1;
        break;
    case PageAnchor::BottomLeft:
        dx = -1;
        dy = 1;
        break;
    case PageAnchor::Bottom:
        dy = 1;
        break;
    case PageAnchor::BottomRight:
        dx = 1;
        dy = 1;
        break;
    }
    return {dx * amount, dy * amount};
}

bool strictBool(QStringView t, bool* out)
{
    const QString s = t.toString().trimmed().toLower();
    if (s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes")) {
        if (out) {
            *out = true;
        }
        return true;
    }
    if (s == QLatin1String("false") || s == QLatin1String("0") || s == QLatin1String("no")) {
        if (out) {
            *out = false;
        }
        return true;
    }
    return false;
}

bool boolWord(QStringView t, bool defaultValue)
{
    if (t.trimmed().isEmpty()) {
        return defaultValue;
    }
    bool v = false;
    if (strictBool(t, &v)) {
        return v;
    }
    return false;
}

QRectF placeRect(const QRectF& bounds, PageAnchor anchor, const PageDimPair& offset,
                 const PageDimPair& size, const QSizeF& screen)
{
    const double bw = bounds.width();
    const double bh = bounds.height();
    const double sw = screen.width() > 0.0 ? screen.width() : bw;
    const double sh = screen.height() > 0.0 ? screen.height() : bh;
    const double w = size.x.isSet() ? size.x.resolve(bw, bh, sw, sh) : 0.0;
    const double h = size.y.isSet() ? size.y.resolve(bh, bh, sw, sh) : 0.0;
    const double ox = offset.x.isSet() ? offset.x.resolve(bw, bh, sw, sh) : 0.0;
    const double oy = offset.y.isSet() ? offset.y.resolve(bh, bh, sw, sh) : 0.0;

    double x = bounds.left();
    double y = bounds.top();
    switch (anchor) {
    case PageAnchor::TopLeft:
        x = bounds.left();
        y = bounds.top();
        break;
    case PageAnchor::Top:
        x = bounds.center().x() - w / 2.0;
        y = bounds.top();
        break;
    case PageAnchor::TopRight:
        x = bounds.right() - w;
        y = bounds.top();
        break;
    case PageAnchor::Left:
        x = bounds.left();
        y = bounds.center().y() - h / 2.0;
        break;
    case PageAnchor::Center:
        x = bounds.center().x() - w / 2.0;
        y = bounds.center().y() - h / 2.0;
        break;
    case PageAnchor::Right:
        x = bounds.right() - w;
        y = bounds.center().y() - h / 2.0;
        break;
    case PageAnchor::BottomLeft:
        x = bounds.left();
        y = bounds.bottom() - h;
        break;
    case PageAnchor::Bottom:
        x = bounds.center().x() - w / 2.0;
        y = bounds.bottom() - h;
        break;
    case PageAnchor::BottomRight:
        x = bounds.right() - w;
        y = bounds.bottom() - h;
        break;
    }
    return QRectF(x + ox, y + oy, w, h);
}

} // namespace PageDimParse
} // namespace gazer
