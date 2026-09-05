#include "assist/SoundboardStore.h"

#include "assist/ElevenClient.h"
#include "assist/SpeechHistory.h"
#include "utils/AtomicFile.h"
#include "utils/Log.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QtGlobal>
#include <QUuid>

namespace gazer {
namespace {

QString readString(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toString();
}

int readInt(const QJsonObject& o, const char* key, int fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble() || v.isString()) {
        return v.toInt(fallback);
    }
    return fallback;
}

SoundboardButton buttonFromJson(const QJsonObject& o)
{
    SoundboardButton b;
    b.id = readString(o, "id").trimmed();
    b.label = readString(o, "label");
    b.icon = readString(o, "icon");
    b.color = readString(o, "color");
    b.sourceText = readString(o, "sourceText");
    b.utteranceText = readString(o, "utteranceText");
    b.clipId = readString(o, "clipId").trimmed();
    b.effectsBaked = o.value(QStringLiteral("effectsBaked")).toBool(false);
    b.col = readInt(o, "col", 0);
    b.row = readInt(o, "row", 0);
    b.colSpan = qMax(1, readInt(o, "colSpan", 1));
    b.rowSpan = qMax(1, readInt(o, "rowSpan", 1));
    return b;
}

QJsonObject buttonToJson(const SoundboardButton& b)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), b.id);
    o.insert(QStringLiteral("label"), b.label);
    o.insert(QStringLiteral("icon"), b.icon);
    o.insert(QStringLiteral("color"), b.color);
    o.insert(QStringLiteral("sourceText"), b.sourceText);
    if (!b.utteranceText.isEmpty()) {
        o.insert(QStringLiteral("utteranceText"), b.utteranceText);
    } else {
        o.insert(QStringLiteral("utteranceText"), QJsonValue::Null);
    }
    o.insert(QStringLiteral("clipId"), b.clipId);
    o.insert(QStringLiteral("effectsBaked"), b.effectsBaked);
    o.insert(QStringLiteral("col"), b.col);
    o.insert(QStringLiteral("row"), b.row);
    o.insert(QStringLiteral("colSpan"), b.colSpan);
    o.insert(QStringLiteral("rowSpan"), b.rowSpan);
    return o;
}

SoundboardTopic topicFromJson(const QJsonObject& o)
{
    SoundboardTopic t;
    t.id = readString(o, "id").trimmed();
    t.name = readString(o, "name");
    t.icon = readString(o, "icon");
    t.color = readString(o, "color");
    t.gridCols = readInt(o, "gridCols", 4);
    t.gridRows = readInt(o, "gridRows", 3);
    const QJsonArray buttons = o.value(QStringLiteral("buttons")).toArray();
    for (const QJsonValue& v : buttons) {
        if (v.isObject()) {
            t.buttons.push_back(buttonFromJson(v.toObject()));
        }
    }
    return t;
}

QJsonObject topicToJson(const SoundboardTopic& t)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), t.id);
    o.insert(QStringLiteral("name"), t.name);
    o.insert(QStringLiteral("icon"), t.icon);
    o.insert(QStringLiteral("color"), t.color);
    o.insert(QStringLiteral("gridCols"), t.gridCols);
    o.insert(QStringLiteral("gridRows"), t.gridRows);
    QJsonArray buttons;
    for (const SoundboardButton& b : t.buttons) {
        buttons.append(buttonToJson(b));
    }
    o.insert(QStringLiteral("buttons"), buttons);
    return o;
}

SoundboardButton textButton(const QString& id, const QString& label, const QString& color)
{
    SoundboardButton b;
    b.id = id;
    b.label = label;
    b.sourceText = label;
    b.utteranceText = label;
    b.color = color;
    return b;
}

} // namespace

SoundboardStore::SoundboardStore(QString rootDir)
    : m_root(std::move(rootDir))
{
    if (m_root.isEmpty()) {
        m_root = ElevenClient::speechDir();
    }
}

QString SoundboardStore::boardsPath() const
{
    return QDir(m_root).filePath(QStringLiteral("boards.json"));
}

QString SoundboardStore::clipsDir() const
{
    return QDir(m_root).filePath(QStringLiteral("clips"));
}

QString SoundboardStore::makeId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

bool SoundboardStore::validClipId(const QString& clipId)
{
    static const QRegularExpression re(QStringLiteral("^[A-Fa-f0-9]{8,32}$"));
    return re.match(clipId).hasMatch();
}

void SoundboardStore::clampTopic(SoundboardTopic* topic)
{
    if (!topic) {
        return;
    }
    topic->gridCols = qBound(1, topic->gridCols, kMaxCols);
    topic->gridRows = qBound(1, topic->gridRows, kMaxRows);
    QVector<SoundboardButton> kept;
    QSet<QString> seen;
    for (SoundboardButton& b : topic->buttons) {
        b.col = qBound(0, b.col, topic->gridCols - 1);
        b.row = qBound(0, b.row, topic->gridRows - 1);
        b.colSpan = qBound(1, b.colSpan, topic->gridCols - b.col);
        b.rowSpan = qBound(1, b.rowSpan, topic->gridRows - b.row);
        if (b.id.isEmpty()) {
            b.id = makeId();
        }
        if (seen.contains(b.id)) {
            continue;
        }
        if (!b.clipId.isEmpty() && !validClipId(b.clipId)) {
            b.clipId.clear();
        }
        seen.insert(b.id);
        kept.push_back(b);
    }
    topic->buttons = std::move(kept);
}

void SoundboardStore::repackSequential(SoundboardTopic* topic)
{
    if (!topic) {
        return;
    }
    clampTopic(topic);
    int i = 0;
    for (SoundboardButton& b : topic->buttons) {
        b.col = i % topic->gridCols;
        b.row = i / topic->gridCols;
        b.colSpan = 1;
        b.rowSpan = 1;
        ++i;
        if (b.row >= topic->gridRows) {
            if (topic->gridRows < kMaxRows) {
                ++topic->gridRows;
            } else {
                break;
            }
        }
    }
    while (!topic->buttons.isEmpty()) {
        const SoundboardButton& last = topic->buttons.last();
        if (last.row < topic->gridRows && last.col < topic->gridCols) {
            break;
        }
        topic->buttons.removeLast();
    }
}

QVector<SoundboardTopic> SoundboardStore::starterTopics()
{
    const QString blue = QStringLiteral("#8AB4F8");
    const QString green = QStringLiteral("#81C995");
    const QString amber = QStringLiteral("#FDD663");
    SoundboardTopic everyday;
    everyday.id = QStringLiteral("starter-everyday");
    everyday.name = QStringLiteral("Everyday");
    everyday.icon = QStringLiteral("chat");
    everyday.color = blue;
    everyday.gridCols = 4;
    everyday.gridRows = 3;
    everyday.buttons = {
        textButton(QStringLiteral("e1"), QStringLiteral("Hello"), blue),
        textButton(QStringLiteral("e2"), QStringLiteral("Yes"), blue),
        textButton(QStringLiteral("e3"), QStringLiteral("No"), blue),
        textButton(QStringLiteral("e4"), QStringLiteral("Please"), blue),
        textButton(QStringLiteral("e5"), QStringLiteral("Thank you"), blue),
        textButton(QStringLiteral("e6"), QStringLiteral("I need help"), blue),
    };
    SoundboardTopic needs;
    needs.id = QStringLiteral("starter-needs");
    needs.name = QStringLiteral("Needs");
    needs.icon = QStringLiteral("ideas");
    needs.color = green;
    needs.gridCols = 4;
    needs.gridRows = 3;
    needs.buttons = {
        textButton(QStringLiteral("n1"), QStringLiteral("Water"), green),
        textButton(QStringLiteral("n2"), QStringLiteral("Bathroom"), green),
        textButton(QStringLiteral("n3"), QStringLiteral("Hungry"), green),
        textButton(QStringLiteral("n4"), QStringLiteral("Pain"), green),
        textButton(QStringLiteral("n5"), QStringLiteral("Tired"), green),
        textButton(QStringLiteral("n6"), QStringLiteral("Break"), green),
    };
    SoundboardTopic feelings;
    feelings.id = QStringLiteral("starter-feelings");
    feelings.name = QStringLiteral("Feelings");
    feelings.icon = QStringLiteral("palette");
    feelings.color = amber;
    feelings.gridCols = 4;
    feelings.gridRows = 3;
    feelings.buttons = {
        textButton(QStringLiteral("f1"), QStringLiteral("Happy"), amber),
        textButton(QStringLiteral("f2"), QStringLiteral("Sad"), amber),
        textButton(QStringLiteral("f3"), QStringLiteral("Okay"), amber),
        textButton(QStringLiteral("f4"), QStringLiteral("Frustrated"), amber),
        textButton(QStringLiteral("f5"), QStringLiteral("Love you"), amber),
        textButton(QStringLiteral("f6"), QStringLiteral("Wait"), amber),
    };
    for (SoundboardTopic* t : {&everyday, &needs, &feelings}) {
        repackSequential(t);
    }
    return {everyday, needs, feelings};
}

bool SoundboardStore::load(QString* error)
{
    QDir().mkpath(m_root);
    QDir().mkpath(clipsDir());
    const QString path = boardsPath();
    QFile f(path);
    if (!f.exists()) {
        m_topics = starterTopics();
        m_activeTopicId = m_topics.isEmpty() ? QString() : m_topics.first().id;
        return save(error);
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read boards.json");
        }
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid boards.json");
        }
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("gazer-soundboard")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        if (error) {
            *error = QStringLiteral("Unknown boards.json format");
        }
        GAZER_WARN << "Soundboard: unknown boards.json format";
        if (m_topics.isEmpty()) {
            m_topics = starterTopics();
            m_activeTopicId = m_topics.isEmpty() ? QString() : m_topics.first().id;
        }
        return false;
    }
    m_topics.clear();
    const QJsonArray topics = root.value(QStringLiteral("topics")).toArray();
    for (const QJsonValue& v : topics) {
        if (!v.isObject()) {
            continue;
        }
        SoundboardTopic t = topicFromJson(v.toObject());
        if (t.id.isEmpty()) {
            continue;
        }
        clampTopic(&t);
        m_topics.push_back(t);
    }
    m_activeTopicId = root.value(QStringLiteral("activeTopicId")).toString();
    if (!findTopic(m_activeTopicId) && !m_topics.isEmpty()) {
        m_activeTopicId = m_topics.first().id;
    }
    if (m_topics.isEmpty()) {
        m_topics = starterTopics();
        m_activeTopicId = m_topics.first().id;
        return save(error);
    }
    return true;
}

bool SoundboardStore::save(QString* error) const
{
    QDir().mkpath(m_root);
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("gazer-soundboard"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("activeTopicId"), m_activeTopicId);
    QJsonArray topics;
    for (const SoundboardTopic& t : m_topics) {
        topics.append(topicToJson(t));
    }
    root.insert(QStringLiteral("topics"), topics);
    return writeFileAtomically(boardsPath(),
                               QJsonDocument(root).toJson(QJsonDocument::Compact), error);
}

SoundboardTopic* SoundboardStore::findTopic(const QString& id)
{
    for (SoundboardTopic& t : m_topics) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

const SoundboardTopic* SoundboardStore::findTopic(const QString& id) const
{
    for (const SoundboardTopic& t : m_topics) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

SoundboardTopic* SoundboardStore::activeTopic()
{
    return findTopic(m_activeTopicId);
}

const SoundboardTopic* SoundboardStore::activeTopic() const
{
    return findTopic(m_activeTopicId);
}

bool SoundboardStore::setActiveTopic(const QString& id)
{
    if (!findTopic(id)) {
        return false;
    }
    m_activeTopicId = id;
    return save();
}

bool SoundboardStore::removeTopic(const QString& id, QString* error)
{
    if (m_topics.size() <= 1) {
        if (error) {
            *error = QStringLiteral("Keep at least one topic");
        }
        return false;
    }
    int idx = -1;
    for (int i = 0; i < m_topics.size(); ++i) {
        if (m_topics[i].id == id) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    m_topics.removeAt(idx);
    if (!findTopic(m_activeTopicId) && !m_topics.isEmpty()) {
        m_activeTopicId = m_topics.first().id;
    }
    if (!save(error)) {
        return false;
    }
    evictUnreferencedClips();
    return true;
}

bool SoundboardStore::setTopicColor(const QString& id, const QString& color, QString* error)
{
    SoundboardTopic* t = findTopic(id);
    if (!t) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    const QString raw = color.trimmed();
    if (raw.isEmpty()) {
        t->color.clear();
        return save(error);
    }
    const QColor c(raw);
    if (!c.isValid()) {
        if (error) {
            *error = QStringLiteral("Invalid color");
        }
        return false;
    }
    t->color = c.name(QColor::HexRgb);
    return save(error);
}

bool SoundboardStore::setTopicIcon(const QString& id, const QString& icon, QString* error)
{
    SoundboardTopic* t = findTopic(id);
    if (!t) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    const QString stem = icon.trimmed();
    if (stem.contains(QLatin1Char('/')) || stem.contains(QLatin1Char('\\'))
        || stem.contains(QStringLiteral(".."))) {
        if (error) {
            *error = QStringLiteral("Invalid icon");
        }
        return false;
    }
    t->icon = stem;
    return save(error);
}

bool SoundboardStore::renameTopic(const QString& id, const QString& name, QString* error)
{
    SoundboardTopic* t = findTopic(id);
    if (!t) {
        if (error) {
            *error = QStringLiteral("Unknown topic");
        }
        return false;
    }
    const QString n = name.trimmed();
    if (n.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Type a name first");
        }
        return false;
    }
    t->name = n;
    return save(error);
}

bool SoundboardStore::newTopic(QString* error)
{
    SoundboardTopic t;
    t.id = QStringLiteral("topic-%1").arg(makeId().left(8));
    int n = 1;
    for (const SoundboardTopic& e : m_topics) {
        if (e.name.startsWith(QLatin1String("Topic "))) {
            bool ok = false;
            n = qMax(n, e.name.mid(6).toInt(&ok) + 1);
        }
    }
    t.name = QStringLiteral("Topic %1").arg(n);
    t.gridCols = 4;
    t.gridRows = 3;
    m_topics.push_back(t);
    m_activeTopicId = t.id;
    return save(error);
}

bool SoundboardStore::loadStarters(QString* error)
{
    SoundboardTopic* t = activeTopic();
    if (!t) {
        if (error) {
            *error = QStringLiteral("No active topic");
        }
        return false;
    }
    if (!t->buttons.isEmpty()) {
        return true;
    }
    const auto starters = starterTopics();
    t->buttons = starters.first().buttons;
    t->gridCols = starters.first().gridCols;
    t->gridRows = starters.first().gridRows;
    repackSequential(t);
    return save(error);
}

SoundboardButton* SoundboardStore::buttonAt(SoundboardTopic& topic, int row, int col)
{
    for (SoundboardButton& b : topic.buttons) {
        if (b.row == row && b.col == col) {
            return &b;
        }
    }
    return nullptr;
}

const SoundboardButton* SoundboardStore::buttonAt(const SoundboardTopic& topic, int row,
                                                  int col) const
{
    for (const SoundboardButton& b : topic.buttons) {
        if (b.row == row && b.col == col) {
            return &b;
        }
    }
    return nullptr;
}

SoundboardButton* SoundboardStore::findButton(const QString& buttonId)
{
    return const_cast<SoundboardButton*>(
        static_cast<const SoundboardStore*>(this)->findButton(buttonId));
}

const SoundboardButton* SoundboardStore::findButton(const QString& buttonId) const
{
    for (const SoundboardTopic& t : m_topics) {
        for (const SoundboardButton& b : t.buttons) {
            if (b.id == buttonId) {
                return &b;
            }
        }
    }
    return nullptr;
}

bool SoundboardStore::removeButton(const QString& buttonId, QString* error)
{
    for (SoundboardTopic& t : m_topics) {
        for (int i = 0; i < t.buttons.size(); ++i) {
            if (t.buttons[i].id != buttonId) {
                continue;
            }
            t.buttons.removeAt(i);
            if (!save(error)) {
                return false;
            }
            evictUnreferencedClips();
            return true;
        }
    }
    if (error) {
        *error = QStringLiteral("Unknown button");
    }
    return false;
}

bool SoundboardStore::setButtonColor(const QString& buttonId, const QString& color, QString* error)
{
    SoundboardButton* b = findButton(buttonId);
    if (!b) {
        if (error) {
            *error = QStringLiteral("Unknown button");
        }
        return false;
    }
    const QString raw = color.trimmed();
    if (raw.isEmpty()) {
        b->color.clear();
        return save(error);
    }
    const QColor c(raw);
    if (!c.isValid()) {
        if (error) {
            *error = QStringLiteral("Invalid color");
        }
        return false;
    }
    b->color = c.name(QColor::HexRgb);
    return save(error);
}

bool SoundboardStore::setButtonIcon(const QString& buttonId, const QString& icon, QString* error)
{
    SoundboardButton* b = findButton(buttonId);
    if (!b) {
        if (error) {
            *error = QStringLiteral("Unknown button");
        }
        return false;
    }
    const QString stem = icon.trimmed();
    if (stem.contains(QLatin1Char('/')) || stem.contains(QLatin1Char('\\'))
        || stem.contains(QStringLiteral(".."))) {
        if (error) {
            *error = QStringLiteral("Invalid icon");
        }
        return false;
    }
    b->icon = stem;
    return save(error);
}

bool SoundboardStore::setButtonLabel(const QString& buttonId, const QString& label, QString* error)
{
    SoundboardButton* b = findButton(buttonId);
    if (!b) {
        if (error) {
            *error = QStringLiteral("Unknown button");
        }
        return false;
    }
    const QString n = label.trimmed();
    if (n.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Type a name first");
        }
        return false;
    }
    b->label = n;
    return save(error);
}

bool SoundboardStore::assignCell(int row, int col, const SoundboardButton& seed, QString* error)
{
    SoundboardTopic* t = activeTopic();
    if (!t) {
        if (error) {
            *error = QStringLiteral("No active topic");
        }
        return false;
    }
    if (row < 0 || col < 0 || col >= t->gridCols) {
        if (error) {
            *error = QStringLiteral("Cell is off the board");
        }
        return false;
    }
    if (row >= t->gridRows) {
        if (t->gridRows >= kMaxRows) {
            if (error) {
                *error = QStringLiteral("Board is full");
            }
            return false;
        }
        t->gridRows = row + 1;
        clampTopic(t);
        if (row >= t->gridRows) {
            if (error) {
                *error = QStringLiteral("Board is full");
            }
            return false;
        }
    }
    SoundboardButton* existing = buttonAt(*t, row, col);
    SoundboardButton b = seed;
    b.col = col;
    b.row = row;
    b.colSpan = 1;
    b.rowSpan = 1;
    const QString oldClip = existing ? existing->clipId : QString();
    if (existing) {
        if (b.id.isEmpty()) {
            b.id = existing->id;
        }
        *existing = b;
    } else {
        if (b.id.isEmpty()) {
            b.id = makeId();
        }
        t->buttons.push_back(b);
    }
    clampTopic(t);
    if (!save(error)) {
        return false;
    }
    if (!oldClip.isEmpty() && oldClip != b.clipId) {
        evictUnreferencedClips();
    }
    return true;
}

SoundboardStore::PlayPlan SoundboardStore::planPlay(const SoundboardButton& b) const
{
    PlayPlan p;
    if (!b.utteranceText.trimmed().isEmpty()) {
        p.kind = PlayPlan::Kind::Utterance;
        p.text = b.utteranceText;
        return p;
    }
    if (!b.clipId.isEmpty()) {
        const QString path = clipPath(b.clipId);
        if (QFileInfo::exists(path)) {
            p.kind = PlayPlan::Kind::Clip;
            p.clipPath = path;
            p.text = b.sourceText;
            return p;
        }
    }
    if (!b.sourceText.trimmed().isEmpty()) {
        p.kind = PlayPlan::Kind::Source;
        p.text = b.sourceText;
        return p;
    }
    return p;
}

QString SoundboardStore::clipPath(const QString& clipId) const
{
    if (!validClipId(clipId)) {
        return {};
    }
    return QDir(clipsDir()).filePath(clipId + QStringLiteral(".mp3"));
}

void SoundboardStore::setMaxTotalBytes(qint64 n)
{
    m_maxBytes = n > 0 ? n : kMaxTotalBytes;
}

QString SoundboardStore::importClip(const QString& srcPath, QString* error, SpeechHistory* history)
{
    QFileInfo src(srcPath);
    if (!src.exists() || !src.isFile()) {
        if (error) {
            *error = QStringLiteral("No clip to pin");
        }
        return {};
    }
    if (src.size() > kMaxClipBytes) {
        if (error) {
            *error = QStringLiteral("Clip is over 5 MB");
        }
        return {};
    }
    QDir().mkpath(clipsDir());
    if (history) {
        history->makeRoom(src.size());
    }
    evictUnreferencedClips();
    const QString id = makeId();
    const QString dest = clipPath(id);
    if (dest.isEmpty() || !QFile::copy(srcPath, dest)) {
        if (error) {
            *error = QStringLiteral("Could not copy clip");
        }
        return {};
    }
    if (usedBytes() > m_maxBytes) {
        QFile::remove(dest);
        if (error) {
            *error = QStringLiteral("Speech storage is full");
        }
        return {};
    }
    return id;
}

QStringList SoundboardStore::referencedClipIds() const
{
    QStringList ids;
    for (const SoundboardTopic& t : m_topics) {
        for (const SoundboardButton& b : t.buttons) {
            if (validClipId(b.clipId)) {
                ids.push_back(b.clipId);
            }
        }
    }
    return ids;
}

qint64 SoundboardStore::usedBytes() const
{
    qint64 n = 0;
    const QStringList dirs = {clipsDir(), QDir(m_root).filePath(QStringLiteral("history"))};
    for (const QString& dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList(QDir::Files);
        for (const QFileInfo& fi : files) {
            n += fi.size();
        }
    }
    return n;
}

void SoundboardStore::evictUnreferencedClips()
{
    const QStringList keep = referencedClipIds();
    const QFileInfoList files =
        QDir(clipsDir()).entryInfoList(QStringList{QStringLiteral("*.mp3")}, QDir::Files,
                                       QDir::Time | QDir::Reversed);
    for (const QFileInfo& fi : files) {
        const QString id = fi.completeBaseName();
        if (!keep.contains(id, Qt::CaseInsensitive)) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

} // namespace gazer
