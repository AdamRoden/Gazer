#include "ui/KeySymbols.h"

#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QCache>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
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
};

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

QByteArray whiteSvg(const QString& d, const QRectF& vb)
{
    QByteArray out;
    out.reserve(180 + d.size());
    out += "<svg xmlns='http://www.w3.org/2000/svg' viewBox='";
    out += QByteArray::number(vb.x(), 'f', 3);
    out += ' ';
    out += QByteArray::number(vb.y(), 'f', 3);
    out += ' ';
    out += QByteArray::number(vb.width(), 'f', 3);
    out += ' ';
    out += QByteArray::number(vb.height(), 'f', 3);
    out += "'><path id='g' fill='#ffffff' fill-rule='evenodd' d='";
    out += d.toUtf8();
    out += "'/></svg>";
    return out;
}

void loadFile(const QString& path, Catalog& cat)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        GAZER_WARN << "KeySymbols: could not read" << path;
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        GAZER_WARN << "KeySymbols: invalid JSON" << path;
        return;
    }
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString d = it.value().toString();
        if (d.isEmpty()) {
            continue;
        }
        QByteArray probe;
        probe += "<svg xmlns='http://www.w3.org/2000/svg'><path id='g' d='";
        probe += d.toUtf8();
        probe += "'/></svg>";
        QSvgRenderer measure(probe);
        if (!measure.isValid()) {
            GAZER_WARN << "KeySymbols: invalid path" << it.key();
            continue;
        }
        QRectF vb = measure.boundsOnElement(QStringLiteral("g"));
        if (vb.isEmpty()) {
            vb = measure.viewBoxF();
        }
        if (vb.isEmpty()) {
            continue;
        }
        auto svg = QSharedPointer<QSvgRenderer>::create();
        if (!svg->load(whiteSvg(d, vb))) {
            continue;
        }
        Glyph g;
        g.svg = std::move(svg);
        g.viewBox = vb;
        cat.glyphs.insert(normalize(it.key()), std::move(g));
        QString display = it.key();
        if (display.endsWith(QLatin1String("Icon"))) {
            display.chop(4);
        }
        cat.displayNames.push_back(display);
    }
}

void loadOnce()
{
    Catalog& cat = catalog();
    if (cat.loaded) {
        return;
    }
    QString path;
    for (const QString& root : resourceIconRoots()) {
        const QString cand = QDir(root).filePath(QStringLiteral("key_symbols.json"));
        if (QFile::exists(cand)) {
            path = cand;
            break;
        }
    }
    if (path.isEmpty()) {
        GAZER_WARN << "KeySymbols: key_symbols.json not found";
        return;
    }
    loadFile(path, cat);
    if (cat.glyphs.isEmpty()) {
        return;
    }
    cat.displayNames.sort(Qt::CaseInsensitive);
    cat.loaded = true;
    GAZER_INFO << "KeySymbols: loaded" << cat.glyphs.size() << "geometries";
}

} // namespace

bool contains(const QString& name)
{
    if (name.isEmpty()) {
        return false;
    }
    loadOnce();
    return catalog().glyphs.contains(normalize(name));
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
    const auto it = cat.glyphs.constFind(normalize(name));
    if (it == cat.glyphs.cend() || !it->svg || !it->svg->isValid()) {
        return false;
    }
    const QRectF& vb = it->viewBox;
    if (vb.isEmpty()) {
        return false;
    }

    const qreal pad = qMin(r.width(), r.height()) * 0.10;
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
    const QString cacheKey = it.key() + QChar(u'#') + QString::number(color.rgba(), 16) + QChar(u'@')
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
        it->svg->render(&ip, QRectF(0, 0, dest.width(), dest.height()));
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
