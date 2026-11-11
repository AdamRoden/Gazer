#include "ui/MaterialIcons.h"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSharedPointer>
#include <QSvgRenderer>
#include <QtMath>

namespace gazer {
namespace MaterialIcons {
namespace {

struct Glyph {
    QSharedPointer<QSvgRenderer> svg;
    QRectF viewBox;
};

struct Cache {
    QHash<QString, Glyph> glyphs;
    QCache<QString, QPixmap> rasters{256};
};

Cache& cache()
{
    static Cache c;
    return c;
}

QString keyOf(const QString& name)
{
    return name.trimmed().toLower();
}

} // namespace

bool contains(const QString& name)
{
    const auto it = cache().glyphs.constFind(keyOf(name));
    return it != cache().glyphs.cend() && it->svg && it->svg->isValid();
}

void store(const QString& name, const QByteArray& svg)
{
    const QString key = keyOf(name);
    if (key.isEmpty() || svg.isEmpty()) {
        return;
    }
    auto renderer = QSharedPointer<QSvgRenderer>::create(svg);
    if (!renderer->isValid()) {
        return;
    }
    Glyph g;
    g.viewBox = renderer->viewBoxF();
    if (g.viewBox.isEmpty()) {
        g.viewBox = QRectF(0, 0, 48, 48);
    }
    g.svg = std::move(renderer);
    cache().glyphs.insert(key, std::move(g));
}

bool paint(QPainter& p, const QString& name, const QRectF& r, const QColor& color)
{
    if (name.isEmpty() || r.isEmpty() || !color.isValid()) {
        return false;
    }
    Cache& cat = cache();
    const auto it = cat.glyphs.constFind(keyOf(name));
    if (it == cat.glyphs.cend() || !it->svg || !it->svg->isValid()) {
        return false;
    }
    const QRectF& vb = it->viewBox;
    if (vb.isEmpty()) {
        return false;
    }
    const qreal pad = qMin(r.width(), r.height()) * 0.08;
    const QRectF box = r.adjusted(pad, pad, -pad, -pad);
    if (box.isEmpty()) {
        return false;
    }
    const qreal s = qMin(box.width() / vb.width(), box.height() / vb.height());
    const QRectF dest(box.center().x() - vb.width() * s * 0.5,
                      box.center().y() - vb.height() * s * 0.5, vb.width() * s, vb.height() * s);
    if (dest.isEmpty()) {
        return false;
    }

    const qreal dpr = p.device() ? qMax(1.0, p.device()->devicePixelRatioF()) : 1.0;
    const int w = qMax(1, qCeil(dest.width() * dpr));
    const int h = qMax(1, qCeil(dest.height() * dpr));
    const QString cacheKey = keyOf(name) + QChar(u'#') + QString::number(color.rgba(), 16)
                             + QChar(u'@') + QString::number(w) + QChar(u'x') + QString::number(h);
    if (const QPixmap* hit = cat.rasters.object(cacheKey)) {
        p.drawPixmap(dest.topLeft(), *hit);
        return true;
    }

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    {
        QPainter ip(&img);
        ip.setRenderHint(QPainter::Antialiasing, true);
        it->svg->render(&ip, QRectF(0, 0, dest.width(), dest.height()));
        ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
        ip.fillRect(QRectF(0, 0, dest.width(), dest.height()), color);
    }
    QPixmap pm = QPixmap::fromImage(std::move(img));
    pm.setDevicePixelRatio(dpr);
    p.drawPixmap(dest.topLeft(), pm);
    cat.rasters.insert(cacheKey, new QPixmap(pm), qMax(1, w * h * 4));
    return true;
}

} // namespace MaterialIcons
} // namespace gazer
