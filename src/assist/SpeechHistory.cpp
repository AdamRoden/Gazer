#include "assist/SpeechHistory.h"

#include "assist/ElevenClient.h"
#include "utils/AtomicFile.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtGlobal>
#include <QUuid>

namespace gazer {
namespace {

SpeechHistoryItem itemFromJson(const QJsonObject& o)
{
    SpeechHistoryItem it;
    it.id = o.value(QStringLiteral("id")).toString().trimmed();
    it.phrase = o.value(QStringLiteral("phrase")).toString();
    it.modelId = o.value(QStringLiteral("modelId")).toString();
    it.voiceId = o.value(QStringLiteral("voiceId")).toString();
    it.backend = o.value(QStringLiteral("backend")).toString();
    it.atIso = o.value(QStringLiteral("at")).toString();
    return it;
}

QJsonObject itemToJson(const SpeechHistoryItem& it)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), it.id);
    o.insert(QStringLiteral("phrase"), it.phrase);
    o.insert(QStringLiteral("modelId"), it.modelId);
    o.insert(QStringLiteral("voiceId"), it.voiceId);
    o.insert(QStringLiteral("backend"), it.backend);
    o.insert(QStringLiteral("at"), it.atIso);
    return o;
}

} // namespace

SpeechHistory::SpeechHistory(QString rootDir)
    : m_root(std::move(rootDir))
{
    if (m_root.isEmpty()) {
        m_root = ElevenClient::speechDir();
    }
}

QString SpeechHistory::jsonPath() const
{
    return QDir(m_root).filePath(QStringLiteral("history.json"));
}

QString SpeechHistory::filesDir() const
{
    return QDir(m_root).filePath(QStringLiteral("history"));
}

QString SpeechHistory::makeId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

bool SpeechHistory::validId(const QString& id)
{
    static const QRegularExpression re(QStringLiteral("^[A-Fa-f0-9]{8,32}$"));
    return re.match(id).hasMatch();
}

QString SpeechHistory::clipPath(const QString& id) const
{
    if (!validId(id)) {
        return {};
    }
    return QDir(filesDir()).filePath(id + QStringLiteral(".mp3"));
}

const SpeechHistoryItem* SpeechHistory::find(const QString& id) const
{
    for (const SpeechHistoryItem& it : m_items) {
        if (it.id == id) {
            return &it;
        }
    }
    return nullptr;
}

bool SpeechHistory::load(QString* error)
{
    QDir().mkpath(m_root);
    QDir().mkpath(filesDir());
    QFile f(jsonPath());
    if (!f.exists()) {
        m_items.clear();
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read history.json");
        }
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid history.json");
        }
        return false;
    }
    m_items.clear();
    const QJsonArray arr = doc.object().value(QStringLiteral("items")).toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        SpeechHistoryItem it = itemFromJson(v.toObject());
        if (it.id.isEmpty() || !validId(it.id) || it.phrase.trimmed().isEmpty()) {
            continue;
        }
        m_items.push_back(it);
        if (m_items.size() >= kMaxItems) {
            break;
        }
    }
    return true;
}

bool SpeechHistory::save(QString* error) const
{
    QDir().mkpath(m_root);
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("gazer-speech-history"));
    root.insert(QStringLiteral("version"), 1);
    QJsonArray arr;
    for (const SpeechHistoryItem& it : m_items) {
        arr.append(itemToJson(it));
    }
    root.insert(QStringLiteral("items"), arr);
    return writeFileAtomically(jsonPath(), QJsonDocument(root).toJson(QJsonDocument::Compact),
                               error);
}

qint64 SpeechHistory::usedBytes() const
{
    qint64 n = 0;
    const QStringList dirs = {filesDir(), QDir(m_root).filePath(QStringLiteral("clips"))};
    for (const QString& dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList(QDir::Files);
        for (const QFileInfo& fi : files) {
            n += fi.size();
        }
    }
    return n;
}

void SpeechHistory::setMaxTotalBytes(qint64 n)
{
    m_maxBytes = n > 0 ? n : kMaxTotalBytes;
}

void SpeechHistory::trimToCap()
{
    while (m_items.size() > kMaxItems) {
        const SpeechHistoryItem last = m_items.takeLast();
        const QString path = clipPath(last.id);
        if (!path.isEmpty()) {
            QFile::remove(path);
        }
    }
    while (usedBytes() > m_maxBytes && !m_items.isEmpty()) {
        const SpeechHistoryItem last = m_items.takeLast();
        const QString path = clipPath(last.id);
        if (!path.isEmpty()) {
            QFile::remove(path);
        }
    }
}

void SpeechHistory::makeRoom(qint64 extraBytes)
{
    const qint64 extra = qMax<qint64>(0, extraBytes);
    while (!m_items.isEmpty() && usedBytes() + extra > m_maxBytes) {
        const SpeechHistoryItem last = m_items.takeLast();
        const QString path = clipPath(last.id);
        if (!path.isEmpty()) {
            QFile::remove(path);
        }
    }
    (void)save();
}

void SpeechHistory::evictOldestHistory()
{
    trimToCap();
    (void)save();
}

bool SpeechHistory::remove(const QString& id, QString* error)
{
    if (!validId(id)) {
        if (error) {
            *error = QStringLiteral("Unknown history item");
        }
        return false;
    }
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id != id) {
            continue;
        }
        const QString path = clipPath(m_items[i].id);
        if (!path.isEmpty()) {
            QFile::remove(path);
        }
        m_items.removeAt(i);
        return save(error);
    }
    if (error) {
        *error = QStringLiteral("Unknown history item");
    }
    return false;
}

bool SpeechHistory::record(const QString& phrase, const QString& backend, const QString& modelId,
                           const QString& voiceId, const QString& mpegPath, QString* error,
                           QString* copiedMpeg)
{
    if (copiedMpeg) {
        copiedMpeg->clear();
    }
    const QString text = phrase.trimmed();
    if (text.isEmpty()) {
        return true;
    }
    SpeechHistoryItem it;
    it.id = makeId();
    it.phrase = phrase;
    it.backend = backend;
    it.modelId = modelId;
    it.voiceId = voiceId;
    it.atIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!mpegPath.isEmpty() && QFileInfo::exists(mpegPath)) {
        QDir().mkpath(filesDir());
        const QString dest = clipPath(it.id);
        if (dest.isEmpty() || !QFile::copy(mpegPath, dest)) {
            if (error) {
                *error = QStringLiteral("Could not copy history clip");
            }
        } else if (copiedMpeg) {
            *copiedMpeg = dest;
        }
    }
    m_items.push_front(it);
    trimToCap();
    return save(error);
}

} // namespace gazer
