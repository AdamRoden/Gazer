#include "ui/KeySymbols.h"

#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QCache>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSharedPointer>
#include <QSvgRenderer>
#include <QtMath>

namespace gazer {
namespace KeySymbols {
namespace {

QString normalize(QString name)
{
    name = name.trimmed();
    if (name.endsWith(QLatin1String("Icon"), Qt::CaseInsensitive)) {
        name.chop(4);
    }
    return name.toLower();
}

struct Glyph {
    QSharedPointer<QSvgRenderer> svg;
    QRectF viewBox;
    QRectF content;
};

/// Opaque ink of @p svg in viewBox coordinates, so paint can fill the cell the
/// way the old path-bounds catalog did. Material canvases and SVGs with no
/// viewBox otherwise sit in a sea of padding and look tiny on keys.
QRectF inkRect(QSvgRenderer& svg, const QRectF& vb)
{
    constexpr int kN = 192;
    QImage img(kN, kN, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        svg.render(&p, QRectF(0, 0, kN, kN));
    }
    int x0 = kN, y0 = kN, x1 = -1, y1 = -1;
    for (int y = 0; y < kN; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < kN; ++x) {
            if (qAlpha(line[x]) > 12) {
                x0 = qMin(x0, x);
                y0 = qMin(y0, y);
                x1 = qMax(x1, x);
                y1 = qMax(y1, y);
            }
        }
    }
    if (x1 < x0) {
        return vb;
    }
    const qreal sx = vb.width() / qreal(kN);
    const qreal sy = vb.height() / qreal(kN);
    QRectF ink(vb.x() + x0 * sx, vb.y() + y0 * sy, (x1 - x0 + 1) * sx, (y1 - y0 + 1) * sy);
    const qreal slack = qMax(ink.width(), ink.height()) * 0.04;
    ink.adjust(-slack, -slack, slack, slack);
    return ink.intersected(vb);
}

struct Catalog {
    QHash<QString, Glyph> glyphs;
    QStringList displayNames;
    QCache<QString, QPixmap> rasters;
    bool loaded = false;

    Catalog() { rasters.setMaxCost(12 * 1024 * 1024); }
};

Catalog& catalog()
{
    static Catalog c;
    return c;
}

QString resolve(const QString& name)
{
    const QString n = normalize(name);
    if (n.isEmpty()) {
        return n;
    }
    const Catalog& cat = catalog();
    if (cat.glyphs.contains(n)) {
        return n;
    }
    return {};
}

void loadOnce()
{
    Catalog& cat = catalog();
    if (cat.loaded) {
        return;
    }
    cat.loaded = true;

    QString dir;
    for (const QString& root : resourceIconRoots()) {
        const QString cand = QDir(root).filePath(QStringLiteral("svg"));
        if (QDir(cand).exists()) {
            dir = cand;
            break;
        }
    }
    if (dir.isEmpty()) {
        GAZER_WARN << "KeySymbols: svg folder not found";
        return;
    }

    const QFileInfoList files =
        QDir(dir).entryInfoList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        auto svg = QSharedPointer<QSvgRenderer>::create(fi.absoluteFilePath());
        if (!svg || !svg->isValid()) {
            GAZER_WARN << "KeySymbols: invalid svg" << fi.fileName();
            continue;
        }
        QRectF vb = svg->viewBoxF();
        if (vb.isEmpty()) {
            const QSize sz = svg->defaultSize();
            if (sz.isValid() && !sz.isEmpty()) {
                vb = QRectF(0, 0, sz.width(), sz.height());
                svg->setViewBox(vb);
            }
        }
        if (vb.isEmpty()) {
            continue;
        }
        Glyph g;
        g.content = inkRect(*svg, vb);
        g.svg = std::move(svg);
        g.viewBox = vb;
        if (g.content.isEmpty()) {
            g.content = vb;
        }
        const QString stem = fi.completeBaseName();
        cat.glyphs.insert(normalize(stem), std::move(g));
        cat.displayNames.push_back(stem);
    }
    cat.displayNames.sort(Qt::CaseInsensitive);
    GAZER_INFO << "KeySymbols: loaded" << cat.glyphs.size() << "svgs from" << dir;
}

} // namespace

bool contains(const QString& name)
{
    if (name.isEmpty()) {
        return false;
    }
    loadOnce();
    return catalog().glyphs.contains(resolve(name));
}

QStringList names()
{
    loadOnce();
    return catalog().displayNames;
}

bool paint(QPainter& p, const QString& name, const QRectF& r, const QColor& color)
{
    if (name.isEmpty() || r.isEmpty() || !color.isValid()) {
        return false;
    }
    loadOnce();
    Catalog& cat = catalog();
    const QString key = resolve(name);
    const auto it = cat.glyphs.constFind(key);
    if (it == cat.glyphs.cend() || !it->svg || !it->svg->isValid()) {
        return false;
    }
    const QRectF& vb = it->viewBox;
    const QRectF content = it->content.isEmpty() ? vb : it->content;
    if (vb.isEmpty() || content.isEmpty()) {
        return false;
    }

    const qreal pad = qMin(r.width(), r.height()) * 0.08;
    const QRectF box = r.adjusted(pad, pad, -pad, -pad);
    if (box.isEmpty()) {
        return false;
    }
    const qreal s = qMin(box.width() / content.width(), box.height() / content.height());
    const QRectF dest(box.center().x() - content.width() * s * 0.5,
                      box.center().y() - content.height() * s * 0.5, content.width() * s,
                      content.height() * s);
    if (dest.isEmpty()) {
        return false;
    }

    const qreal dpr = p.device() ? qMax(1.0, p.device()->devicePixelRatioF()) : 1.0;
    const int w = qMax(1, qCeil(dest.width() * dpr));
    const int h = qMax(1, qCeil(dest.height() * dpr));
    const QString cacheKey = key + QChar(u'#') + QString::number(color.rgba(), 16) + QChar(u'@')
                             + QString::number(w) + QChar(u'x') + QString::number(h);
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
        // Map the full viewBox so `content` fills the image.
        const QRectF renderRect(-(content.x() - vb.x()) / content.width() * dest.width(),
                                -(content.y() - vb.y()) / content.height() * dest.height(),
                                vb.width() / content.width() * dest.width(),
                                vb.height() / content.height() * dest.height());
        it->svg->render(&ip, renderRect);
        ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
        ip.fillRect(QRectF(0, 0, dest.width(), dest.height()), color);
    }
    QPixmap pm = QPixmap::fromImage(std::move(img));
    pm.setDevicePixelRatio(dpr);
    p.drawPixmap(dest.topLeft(), pm);
    cat.rasters.insert(cacheKey, new QPixmap(pm), w * h * 4);
    return true;
}

} // namespace KeySymbols
} // namespace gazer
