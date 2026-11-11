#include "app/IconCatalog.h"

#include "ui/KeySymbols.h"
#include "ui/MaterialIcons.h"
#include "utils/AppDirs.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <utility>

namespace gazer {

namespace {

constexpr int kResultLimit = 120;
constexpr int kSvgInflight = 4;
constexpr auto kGoogleUrl = "https://fonts.google.com/metadata/icons?incomplete=1";
constexpr auto kGithubUrl =
    "https://raw.githubusercontent.com/marella/material-symbols/main/material-symbols/index.d.ts";

QString iconRoot()
{
    const QString dir = QDir(AppDirs::localRoot()).filePath(QStringLiteral("icons"));
    QDir().mkpath(QDir(dir).filePath(QStringLiteral("svg")));
    return dir;
}

bool legalIconName(const QString& name)
{
    if (name.isEmpty() || name.size() > 80) {
        return false;
    }
    for (const QChar c : name) {
        if (c.isDigit() || c == u'_') {
            continue;
        }
        if (!c.isLetter() || !c.isLower()) {
            return false;
        }
    }
    return true;
}

QNetworkRequest jsonRequest(const QString& url, int timeoutMs)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Gazer"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(timeoutMs);
    return req;
}

QVector<IconEntry> parseGoogle(const QByteArray& body)
{
    QByteArray raw = body;
    const int brace = raw.indexOf('{');
    if (brace > 0) {
        raw = raw.mid(brace);
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonArray icons = doc.object().value(QStringLiteral("icons")).toArray();
    QVector<IconEntry> out;
    out.reserve(icons.size());
    for (const QJsonValue& v : icons) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (!legalIconName(name)) {
            continue;
        }
        QStringList words;
        words.push_back(name);
        for (const QJsonValue& tag : o.value(QStringLiteral("tags")).toArray()) {
            words.push_back(tag.toString());
        }
        for (const QJsonValue& cat : o.value(QStringLiteral("categories")).toArray()) {
            words.push_back(cat.toString());
        }
        IconEntry e;
        e.name = name;
        e.hay = words.join(QLatin1Char(' ')).toLower();
        e.popularity = o.value(QStringLiteral("popularity")).toInt();
        out.push_back(std::move(e));
    }
    return out;
}

QVector<IconEntry> parseGithub(const QByteArray& body)
{
    QVector<IconEntry> out;
    QSet<QString> seen;
    static const QRegularExpression re(QStringLiteral("\"([a-z0-9_]+)\""));
    auto it = re.globalMatch(QString::fromUtf8(body));
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!legalIconName(name) || seen.contains(name)) {
            continue;
        }
        seen.insert(name);
        IconEntry e;
        e.name = name;
        e.hay = name;
        out.push_back(std::move(e));
    }
    return out;
}

QVector<IconEntry> fallbackCatalog()
{
    struct Seed {
        const char* name;
        const char* tags;
        int popularity;
    };
    static constexpr Seed kSeeds[] = {
        {"chat", "message talk speech bubble communication", 1000000},
        {"folder", "topic category directory storage", 900000},
        {"history", "time clock watch schedule recent", 900000},
        {"settings", "gear cog preferences options", 900000},
        {"volume_up", "speaker audio sound volume play music", 900000},
        {"home", "house building living residence", 900000},
        {"star", "favorite rating important badge", 900000},
        {"restaurant", "food burger eat dining meal", 800000},
        {"local_pizza", "pizza slice italian food dinner", 800000},
        {"local_cafe", "coffee tea drink cup beverage morning", 800000},
        {"directions_car", "car vehicle travel drive transport automobile", 800000},
        {"flight", "airplane travel vacation fly", 800000},
        {"sports_esports", "game play controller fun hobby entertainment", 800000},
        {"medical_services", "doctor medical hospital stethoscope health illness", 800000},
        {"medication", "pill medicine drug prescription pharmacy health", 800000},
        {"shopping_cart", "store groceries market buy", 800000},
        {"music_note", "song audio melody rhythm", 800000},
        {"lightbulb", "idea bright thought electricity", 800000},
        {"pets", "dog cat animal paw footprint", 800000},
        {"call", "phone contact telephone dial", 800000},
        {"favorite", "heart love like care feeling emotion", 800000},
        {"sentiment_satisfied", "happy smile face joy glad emotion good", 800000},
        {"sentiment_dissatisfied", "sad cry tears upset unhappy feeling", 800000},
        {"bed", "sleep tired nap rest exhausted", 800000},
        {"priority_high", "exclamation alert attention warning important", 800000},
        {"help", "question ask query doubt what", 800000},
        {"workspace_premium", "glowing star bright excellent winner", 800000},
        {"celebration", "party congrats birthday festive", 800000},
        {"visibility", "eyes look see watch observe vision", 800000},
        {"hearing", "ear listen sound deaf auditory", 800000},
        {"psychology", "brain mind think intelligence idea", 800000},
        {"bolt", "lightning fast power energy electric speed", 800000},
        {"trophy", "win award achievement success goal", 800000},
        {"edit", "note write pencil text document", 800000},
        {"push_pin", "pin tack location assign fix remember", 800000},
        {"delete", "trash garbage remove dispose clear", 800000},
        {"key", "lock password access security safe", 800000},
        {"directions_walk", "walk pedestrian go step move exercise", 800000},
        {"thumb_up", "thumbs yes approve like good okay", 800000},
        {"thumb_down", "thumbs no disapprove bad reject", 800000},
        {"recycling", "recycle reuse environment arrows", 800000},
        {"search", "find magnifying glass query", 700000},
    };
    QVector<IconEntry> out;
    out.reserve(int(std::size(kSeeds)));
    for (const Seed& s : kSeeds) {
        IconEntry e;
        e.name = QString::fromLatin1(s.name);
        e.hay = e.name + QLatin1Char(' ') + QString::fromLatin1(s.tags);
        e.popularity = s.popularity;
        out.push_back(std::move(e));
    }
    return out;
}

} // namespace

struct IconCatalog::Store {
    QVector<IconEntry> entries;
    QSet<QString> known;
    QString source = QStringLiteral("offline favorites");
    bool loading = false;
    bool started = false;
    bool haveCatalog = false;
    int generation = 0;
    mutable QString cachedQuery;
    mutable QStringList cachedHits;
    mutable int cacheGen = -1;
    QNetworkAccessManager* nam = nullptr;
    QStringList queue;
    QSet<QString> queued;
    QSet<QString> inflight;
    QSet<QString> failed;
    QTimer* svgTimer = nullptr;
};

IconCatalog::IconCatalog(QObject* parent)
    : QObject(parent)
    , m_store(new Store)
{
    m_store->nam = new QNetworkAccessManager(this);
    m_store->svgTimer = new QTimer(this);
    m_store->svgTimer->setSingleShot(true);
    m_store->svgTimer->setInterval(40);
    connect(m_store->svgTimer, &QTimer::timeout, this, [this]() { emit iconsReady(); });
    m_store->entries = fallbackCatalog();
    for (const IconEntry& e : m_store->entries) {
        m_store->known.insert(e.name);
    }
}

IconCatalog::~IconCatalog()
{
    m_alive = false;
    const auto replies = findChildren<QNetworkReply*>();
    for (QNetworkReply* reply : replies) {
        reply->disconnect(this);
        reply->abort();
    }
    delete m_store;
    m_store = nullptr;
}

void IconCatalog::ensureLoaded()
{
    if (!m_alive || m_store->started) {
        return;
    }
    m_store->started = true;
    m_store->loading = true;
    QTimer::singleShot(0, this, [this]() { loadDiskThenNetwork(); });
}

QString IconCatalog::statusLabel() const
{
    if (m_store->loading && !m_store->haveCatalog) {
        return QStringLiteral("Loading icons…");
    }
    return m_store->source;
}

QStringList IconCatalog::search(const QString& query) const
{
    const Store& s = *m_store;
    const QString q = query.trimmed().toLower();
    if (s.cacheGen == s.generation && s.cachedQuery == q) {
        return s.cachedHits;
    }
    QVector<int> idx;
    idx.reserve(s.entries.size());
    for (int i = 0; i < s.entries.size(); ++i) {
        const IconEntry& e = s.entries.at(i);
        if (q.isEmpty() || e.name.contains(q) || e.hay.contains(q)) {
            idx.push_back(i);
        }
    }
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        const IconEntry& ea = s.entries.at(a);
        const IconEntry& eb = s.entries.at(b);
        if (!q.isEmpty()) {
            const auto rank = [&](const IconEntry& e) {
                if (e.name == q) {
                    return 0;
                }
                if (e.name.startsWith(q)) {
                    return 1;
                }
                if (e.name.contains(q)) {
                    return 2;
                }
                return 3;
            };
            const int ra = rank(ea);
            const int rb = rank(eb);
            if (ra != rb) {
                return ra < rb;
            }
        }
        if (ea.popularity != eb.popularity) {
            return ea.popularity > eb.popularity;
        }
        return ea.name < eb.name;
    });
    if (idx.size() > kResultLimit) {
        idx.resize(kResultLimit);
    }
    QStringList out;
    out.reserve(idx.size());
    for (int i : idx) {
        out.push_back(s.entries.at(i).name);
    }
    s.cachedQuery = q;
    s.cachedHits = out;
    s.cacheGen = s.generation;
    return out;
}

void IconCatalog::install(QVector<IconEntry> entries, const QString& source)
{
    if (!m_alive || entries.isEmpty()) {
        return;
    }
    Store& s = *m_store;
    s.entries = std::move(entries);
    s.known.clear();
    for (const IconEntry& e : s.entries) {
        s.known.insert(e.name);
    }
    s.source = source;
    s.haveCatalog = true;
    ++s.generation;
    emit catalogChanged();
}

void IconCatalog::loadDiskThenNetwork()
{
    QFile file(QDir(iconRoot()).filePath(QStringLiteral("catalog.json")));
    if (file.open(QIODevice::ReadOnly)) {
        QVector<IconEntry> entries = parseGoogle(file.readAll());
        if (!entries.isEmpty()) {
            install(std::move(entries), QStringLiteral("fonts.google.com/icons"));
        }
    }
    fetchGoogle();
}

void IconCatalog::fetchGoogle()
{
    if (!m_alive || !m_store->nam) {
        return;
    }
    QNetworkReply* reply = m_store->nam->get(jsonRequest(QLatin1String(kGoogleUrl), 30000));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_alive) {
            return;
        }
        const QByteArray body = reply->readAll();
        QVector<IconEntry> entries;
        if (reply->error() == QNetworkReply::NoError) {
            entries = parseGoogle(body);
        }
        if (entries.isEmpty()) {
            if (!m_store->haveCatalog) {
                fetchGithub();
            } else {
                m_store->loading = false;
                emit catalogChanged();
            }
            if (reply->error() != QNetworkReply::NoError) {
                GAZER_WARN << "Icon catalog:" << reply->errorString();
            }
            return;
        }
        QSaveFile out(QDir(iconRoot()).filePath(QStringLiteral("catalog.json")));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(body);
            out.commit();
        }
        m_store->loading = false;
        install(std::move(entries), QStringLiteral("fonts.google.com/icons"));
    });
}

void IconCatalog::fetchGithub()
{
    if (!m_alive || !m_store->nam) {
        return;
    }
    QNetworkReply* reply = m_store->nam->get(jsonRequest(QLatin1String(kGithubUrl), 30000));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (!m_alive) {
            return;
        }
        QVector<IconEntry> entries;
        if (reply->error() == QNetworkReply::NoError) {
            entries = parseGithub(reply->readAll());
        }
        m_store->loading = false;
        if (entries.isEmpty()) {
            m_store->source = QStringLiteral("offline favorites");
            emit catalogChanged();
            if (reply->error() != QNetworkReply::NoError) {
                GAZER_WARN << "Icon catalog mirror:" << reply->errorString();
            }
            return;
        }
        install(std::move(entries), QStringLiteral("Material Symbols"));
    });
}

void IconCatalog::requestIcons(const QStringList& names)
{
    if (!m_alive) {
        return;
    }
    const QString dir = QDir(iconRoot()).filePath(QStringLiteral("svg"));
    for (const QString& raw : names) {
        const QString name = raw.trimmed().toLower();
        if (!legalIconName(name) || !m_store->known.contains(name)) {
            continue;
        }
        if (KeySymbols::contains(name) || MaterialIcons::contains(name)
            || m_store->failed.contains(name) || m_store->inflight.contains(name)
            || m_store->queued.contains(name)) {
            continue;
        }
        QFile file(QDir(dir).filePath(name + QStringLiteral(".svg")));
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            if (bytes.contains("<svg")) {
                MaterialIcons::store(name, bytes);
                continue;
            }
        }
        m_store->queue.push_back(name);
        m_store->queued.insert(name);
    }
    pump();
}

void IconCatalog::pump()
{
    while (m_alive && m_store->inflight.size() < kSvgInflight && !m_store->queue.isEmpty()) {
        const QString name = m_store->queue.takeFirst();
        m_store->queued.remove(name);
        fetchSvg(name);
    }
}

void IconCatalog::fetchSvg(const QString& name)
{
    if (!m_alive || !m_store->nam || !legalIconName(name)) {
        return;
    }
    m_store->inflight.insert(name);
    const QString url =
        QStringLiteral("https://fonts.gstatic.com/s/i/short-term/release/materialsymbolsoutlined/"
                       "%1/default/48px.svg")
            .arg(name);
    QNetworkReply* reply = m_store->nam->get(jsonRequest(url, 20000));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        reply->deleteLater();
        if (!m_alive) {
            return;
        }
        m_store->inflight.remove(name);
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !body.contains("<svg")) {
            m_store->failed.insert(name);
        } else {
            MaterialIcons::store(name, body);
            QSaveFile out(QDir(iconRoot()).filePath(QStringLiteral("svg/") + name
                                                     + QStringLiteral(".svg")));
            if (out.open(QIODevice::WriteOnly)) {
                out.write(body);
                out.commit();
            }
            if (!m_store->svgTimer->isActive()) {
                m_store->svgTimer->start();
            }
        }
        pump();
    });
}

} // namespace gazer
