#include "layout/PageDim.h"

#include <QStringList>

namespace gazer {
namespace PageDimParse {

namespace {

QString norm(const QString& s)
{
    return s.trimmed().toLower();
}

} // namespace

PageDim parse(const QString& token, QString* error)
{
    QString t = token.trimmed();
    if (t.isEmpty()) {
        return {};
    }

    bool heightRel = false;
    if (t.size() > 1 && (t.endsWith(QLatin1Char('h')) || t.endsWith(QLatin1Char('H')))) {
        heightRel = true;
        t.chop(1);
        t = t.trimmed();
    }

    PageDim dim;
    if (t.contains(QLatin1Char('/'))) {
        const QStringList parts = t.split(QLatin1Char('/'));
        if (parts.size() != 2) {
            if (error) {
                *error = QStringLiteral("Invalid fraction '%1'").arg(token);
            }
            return {};
        }
        bool okNum = false;
        bool okDen = false;
        const double num = parts[0].trimmed().toDouble(&okNum);
        const double den = parts[1].trimmed().toDouble(&okDen);
        if (!okNum || !okDen || den == 0.0) {
            if (error) {
                *error = QStringLiteral("Invalid fraction '%1'").arg(token);
            }
            return {};
        }
        dim = PageDim::proportion(num / den);
    } else {
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Invalid dimension '%1'").arg(token);
            }
            return {};
        }
        dim = t.contains(QLatin1Char('.')) ? PageDim::proportion(v) : PageDim::pixels(v);
    }
    if (heightRel) {
        if (dim.unit == PageDim::Unit::Pixels) {
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
    if (n.isEmpty() || n == QLatin1String("topleft")) {
        return PageAnchor::TopLeft;
    }
    if (n == QLatin1String("top") || n == QLatin1String("topcenter")) {
        return PageAnchor::Top;
    }
    if (n == QLatin1String("topright")) {
        return PageAnchor::TopRight;
    }
    if (n == QLatin1String("left") || n == QLatin1String("leftcenter")
        || n == QLatin1String("centerleft")) {
        return PageAnchor::Left;
    }
    if (n == QLatin1String("center")) {
        return PageAnchor::Center;
    }
    if (n == QLatin1String("right") || n == QLatin1String("rightcenter")
        || n == QLatin1String("centerright")) {
        return PageAnchor::Right;
    }
    if (n == QLatin1String("bottomleft")) {
        return PageAnchor::BottomLeft;
    }
    if (n == QLatin1String("bottom") || n == QLatin1String("bottomcenter")) {
        return PageAnchor::Bottom;
    }
    if (n == QLatin1String("bottomright")) {
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

QRectF placeRect(const QRectF& bounds, PageAnchor anchor, const PageDimPair& offset,
                 const PageDimPair& size)
{
    const double bw = bounds.width();
    const double bh = bounds.height();
    const double w = size.x.isSet() ? size.x.resolve(bw, bh) : 0.0;
    const double h = size.y.isSet() ? size.y.resolve(bh, bh) : 0.0;
    const double ox = offset.x.isSet() ? offset.x.resolve(bw, bh) : 0.0;
    const double oy = offset.y.isSet() ? offset.y.resolve(bh, bh) : 0.0;

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
