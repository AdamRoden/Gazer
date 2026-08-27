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

[[nodiscard]] bool isNumericToken(const QString& t)
{
    if (t.isEmpty()) {
        return false;
    }
    bool ok = false;
    (void)t.toDouble(&ok);
    if (!ok) {
        return false;
    }
    for (const QChar c : t) {
        if (c.isLetter() && c != QLatin1Char('e') && c != QLatin1Char('E')) {
            return false;
        }
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
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() != 2) {
        if (error) {
            *error = QStringLiteral("Expected x,y pair, got '%1'").arg(s);
        }
        return {};
    }
    PageDimPair out;
    QString err;
    out.x = parse(parts[0], &err);
    if (!err.isEmpty()) {
        if (error) {
            *error = err;
        }
        return {};
    }
    out.y = parse(parts[1], &err);
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
