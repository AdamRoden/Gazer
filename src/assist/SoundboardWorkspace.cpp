#include "assist/SoundboardStore.h"

#include "assist/SoundboardJson.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QtGlobal>

namespace gazer {

using soundboard_json::buttonFromJson;
using soundboard_json::readInt;
using soundboard_json::readString;

namespace {

QJsonValue textOrNull(const QString& text)
{
    return text.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(text);
}

bool looseDouble(const QJsonValue& v, double* out)
{
    if (!out) {
        return false;
    }
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    if (v.isString()) {
        bool ok = false;
        const double n = v.toString().trimmed().toDouble(&ok);
        if (ok) {
            *out = n;
            return true;
        }
    }
    return false;
}

bool topicNeedsRepack(const SoundboardTopic& topic)
{
    if (topic.gridCols < 1 || topic.gridRows < 1 || topic.gridCols > SoundboardStore::kMaxCols
        || topic.gridRows > SoundboardStore::kMaxRows) {
        return true;
    }
    for (const SoundboardButton& b : topic.buttons) {
        if (b.col < 0 || b.row < 0 || b.colSpan < 1 || b.rowSpan < 1) {
            return true;
        }
        if (b.col >= topic.gridCols || b.row >= topic.gridRows) {
            return true;
        }
        if (b.col + b.colSpan > topic.gridCols || b.row + b.rowSpan > topic.gridRows) {
            return true;
        }
    }
    return false;
}

void finishTopic(SoundboardTopic* topic)
{
    if (!topic) {
        return;
    }
    if (topicNeedsRepack(*topic)) {
        SoundboardStore::repackSequential(topic);
    } else {
        SoundboardStore::clampTopic(topic);
    }
}

QString clipDataUrl(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty() || bytes.size() > SoundboardStore::kMaxClipBytes) {
        return {};
    }
    const bool wav = path.endsWith(QLatin1String(".wav"), Qt::CaseInsensitive);
    const QString mime = wav ? QStringLiteral("audio/wav") : QStringLiteral("audio/mpeg");
    return QStringLiteral("data:") + mime + QStringLiteral(";base64,")
           + QString::fromLatin1(bytes.toBase64());
}

SoundboardButton ingestWorkspaceButton(const QJsonObject& o)
{
    SoundboardButton b = buttonFromJson(o);
    const bool explicitUtterance = !readString(o, "utteranceText").trimmed().isEmpty();
    const QString audio = o.value(QStringLiteral("audioData")).toString();
    const bool hasAudio = audio.startsWith(QLatin1String("data:"));
    if (!explicitUtterance && hasAudio) {
        if (b.sourceText.trimmed().isEmpty()) {
            b.sourceText = b.label.trimmed();
        }
        b.utteranceText.clear();
        b.clipId.clear();
    } else if (b.sourceText.trimmed().isEmpty() && b.utteranceText.trimmed().isEmpty()) {
        const QString label = b.label.trimmed();
        if (!label.isEmpty()) {
            b.sourceText = label;
            b.utteranceText = label;
        }
    }
    return b;
}

} // namespace

QByteArray SoundboardStore::exportWorkspace(const WorkspaceExportSettings& settings,
                                            bool* audioOmitted, qint64 maxBytes) const
{
    const auto build = [&](bool withAudio) {
        QJsonObject root;
        root.insert(QStringLiteral("format"), QStringLiteral("aac-workspace"));
        root.insert(QStringLiteral("version"), 1);
        root.insert(QStringLiteral("exportedAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        root.insert(QStringLiteral("activeTopicId"), m_activeTopicId);
        QJsonArray topics;
        for (const SoundboardTopic& topic : m_topics) {
            QJsonObject to;
            to.insert(QStringLiteral("id"), topic.id);
            to.insert(QStringLiteral("name"), topic.name);
            to.insert(QStringLiteral("icon"), topic.icon);
            to.insert(QStringLiteral("color"), topic.color);
            to.insert(QStringLiteral("thread"), QStringLiteral("all"));
            to.insert(QStringLiteral("gridCols"), topic.gridCols);
            to.insert(QStringLiteral("gridRows"), topic.gridRows);
            QJsonArray buttons;
            for (const SoundboardButton& b : topic.buttons) {
                QJsonObject bo;
                bo.insert(QStringLiteral("id"), b.id);
                bo.insert(QStringLiteral("label"), b.label);
                bo.insert(QStringLiteral("symbol"), b.icon);
                bo.insert(QStringLiteral("icon"), b.icon);
                bo.insert(QStringLiteral("color"), b.color);
                bo.insert(QStringLiteral("sourceText"), textOrNull(b.sourceText));
                bo.insert(QStringLiteral("utteranceText"), textOrNull(b.utteranceText));
                QString audio;
                if (withAudio && validClipId(b.clipId)) {
                    const QString path = clipPath(b.clipId);
                    if (QFileInfo::exists(path)) {
                        audio = clipDataUrl(path);
                    }
                }
                bo.insert(QStringLiteral("audioData"),
                          audio.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(audio));
                bo.insert(QStringLiteral("effectsBaked"), withAudio && !audio.isEmpty() && b.effectsBaked);
                bo.insert(QStringLiteral("col"), b.col);
                bo.insert(QStringLiteral("row"), b.row);
                bo.insert(QStringLiteral("colSpan"), b.colSpan);
                bo.insert(QStringLiteral("rowSpan"), b.rowSpan);
                buttons.append(bo);
            }
            to.insert(QStringLiteral("buttons"), buttons);
            topics.append(to);
        }
        root.insert(QStringLiteral("topics"), topics);
        QJsonObject speech;
        speech.insert(QStringLiteral("theme"), settings.theme);
        speech.insert(QStringLiteral("accentColor"), settings.accentColor);
        speech.insert(QStringLiteral("model"), settings.model);
        speech.insert(QStringLiteral("speed"), settings.speed);
        speech.insert(QStringLiteral("pitch"), settings.pitch);
        speech.insert(QStringLiteral("volume"),
                      QString::number(qBound(1, settings.volumeStop, 5)));
        root.insert(QStringLiteral("settings"), speech);
        if (!withAudio) {
            root.insert(QStringLiteral("exportNote"),
                        QStringLiteral(
                            "Audio clips omitted (file too large); phrases export as live text."));
        }
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    };

    QByteArray json = build(true);
    bool omitted = false;
    if (maxBytes > 0 && json.size() > maxBytes) {
        json = build(false);
        omitted = true;
    }
    if (audioOmitted) {
        *audioOmitted = omitted;
    }
    return json;
}

bool SoundboardStore::importWorkspace(const QJsonObject& root, WorkspaceMode mode,
                                      WorkspaceImportedSettings* imported, QString* error)
{
    if (error) {
        error->clear();
    }
    const auto fail = [&](const QString& message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    if (root.isEmpty()) {
        return fail(QStringLiteral("Invalid file"));
    }
    const QString format = root.value(QStringLiteral("format")).toString();
    if (!format.isEmpty() && format != QLatin1String("aac-workspace")
        && format != QLatin1String("gazer-soundboard")) {
        return fail(QStringLiteral("Not an AAC Workspace export file"));
    }
    const QJsonValue topicsVal = root.value(QStringLiteral("topics"));
    if (!topicsVal.isArray() || topicsVal.toArray().isEmpty()) {
        return fail(QStringLiteral("Export has no topics"));
    }

    WorkspaceImportedSettings parsed;
    if (root.value(QStringLiteral("settings")).isObject()) {
        const QJsonObject speech = root.value(QStringLiteral("settings")).toObject();
        const QString model = speech.value(QStringLiteral("model")).toString().trimmed();
        if (!model.isEmpty()) {
            parsed.hasModel = true;
            parsed.model = model;
        }
        double number = 0;
        if (looseDouble(speech.value(QStringLiteral("speed")), &number)) {
            parsed.hasSpeed = true;
            parsed.speed = number;
        }
        if (looseDouble(speech.value(QStringLiteral("pitch")), &number)) {
            parsed.hasPitch = true;
            parsed.pitch = number;
        }
        if (looseDouble(speech.value(QStringLiteral("volume")), &number)) {
            parsed.hasVolume = true;
            parsed.volume = qBound(1, qRound(number), 5);
        }
    }

    QVector<SoundboardTopic> next = mode == WorkspaceMode::Merge ? m_topics : QVector<SoundboardTopic>{};
    QHash<QString, QString> audioById;

    const auto topicTaken = [&](const QString& id) {
        for (const SoundboardTopic& topic : next) {
            if (topic.id == id) {
                return true;
            }
        }
        return false;
    };
    const auto buttonTaken = [&](const QString& id) {
        for (const SoundboardTopic& topic : next) {
            for (const SoundboardButton& button : topic.buttons) {
                if (button.id == id) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto rememberAudio = [&](const QString& id, const QJsonObject& raw) {
        if (!readString(raw, "utteranceText").trimmed().isEmpty()) {
            return;
        }
        const QString data = raw.value(QStringLiteral("audioData")).toString();
        if (data.startsWith(QLatin1String("data:"))) {
            audioById.insert(id, data);
        }
    };

    bool keptOne = false;
    for (const QJsonValue& topicVal : topicsVal.toArray()) {
        if (!topicVal.isObject()) {
            continue;
        }
        const QJsonObject rawTopic = topicVal.toObject();
        const QString rawTopicId = readString(rawTopic, "id").trimmed();
        SoundboardTopic* existing = nullptr;
        if (mode == WorkspaceMode::Merge && !rawTopicId.isEmpty()) {
            for (SoundboardTopic& topic : next) {
                if (topic.id == rawTopicId) {
                    existing = &topic;
                    break;
                }
            }
        }
        if (!existing) {
            SoundboardTopic incoming;
            incoming.id = rawTopicId;
            incoming.name = readString(rawTopic, "name");
            incoming.icon = readString(rawTopic, "icon").trimmed();
            if (incoming.icon.isEmpty()) {
                incoming.icon = readString(rawTopic, "symbol").trimmed();
            }
            incoming.color = readString(rawTopic, "color");
            incoming.gridCols = readInt(rawTopic, "gridCols", 4);
            incoming.gridRows = readInt(rawTopic, "gridRows", 3);
            if (incoming.name.isEmpty()) {
                incoming.name = QStringLiteral("Topic");
            }
            while (incoming.id.isEmpty() || topicTaken(incoming.id)) {
                incoming.id = makeId();
            }
            const QJsonArray buttons = rawTopic.value(QStringLiteral("buttons")).toArray();
            for (const QJsonValue& buttonVal : buttons) {
                if (!buttonVal.isObject()) {
                    continue;
                }
                const QJsonObject rawButton = buttonVal.toObject();
                SoundboardButton button = ingestWorkspaceButton(rawButton);
                while (button.id.isEmpty() || buttonTaken(button.id)) {
                    button.id = makeId();
                }
                rememberAudio(button.id, rawButton);
                incoming.buttons.push_back(button);
            }
            finishTopic(&incoming);
            next.push_back(incoming);
            keptOne = true;
            continue;
        }

        if (!readString(rawTopic, "name").trimmed().isEmpty()) {
            existing->name = readString(rawTopic, "name").trimmed();
        }
        const QString icon = readString(rawTopic, "icon").trimmed();
        if (!icon.isEmpty()) {
            existing->icon = icon;
        }
        const QString color = readString(rawTopic, "color").trimmed();
        if (!color.isEmpty()) {
            existing->color = color;
        }
        if (rawTopic.contains(QLatin1String("gridCols"))) {
            existing->gridCols = qMax(existing->gridCols, readInt(rawTopic, "gridCols", existing->gridCols));
        }
        if (rawTopic.contains(QLatin1String("gridRows"))) {
            existing->gridRows = qMax(existing->gridRows, readInt(rawTopic, "gridRows", existing->gridRows));
        }
        const QJsonArray buttons = rawTopic.value(QStringLiteral("buttons")).toArray();
        for (const QJsonValue& buttonVal : buttons) {
            if (!buttonVal.isObject()) {
                continue;
            }
            const QJsonObject rawButton = buttonVal.toObject();
            SoundboardButton norm = ingestWorkspaceButton(rawButton);
            const QString rawId = readString(rawButton, "id").trimmed();
            SoundboardButton* dest = nullptr;
            if (!rawId.isEmpty()) {
                for (SoundboardButton& button : existing->buttons) {
                    if (button.id == rawId) {
                        dest = &button;
                        break;
                    }
                }
            }
            if (!dest) {
                while (norm.id.isEmpty() || buttonTaken(norm.id)) {
                    norm.id = makeId();
                }
                rememberAudio(norm.id, rawButton);
                existing->buttons.push_back(norm);
                continue;
            }
            if (rawButton.contains(QLatin1String("label"))) {
                dest->label = norm.label;
            }
            if (rawButton.contains(QLatin1String("symbol")) || rawButton.contains(QLatin1String("icon"))) {
                dest->icon = norm.icon;
            }
            if (rawButton.contains(QLatin1String("color"))) {
                dest->color = norm.color;
            }
            if (rawButton.contains(QLatin1String("utteranceText"))) {
                dest->utteranceText = norm.utteranceText;
            }
            if (rawButton.contains(QLatin1String("sourceText"))) {
                dest->sourceText = norm.sourceText;
            }
            if (rawButton.contains(QLatin1String("text"))
                && !rawButton.contains(QLatin1String("utteranceText"))
                && !rawButton.contains(QLatin1String("sourceText"))) {
                dest->sourceText = norm.sourceText;
                if (dest->clipId.isEmpty()) {
                    dest->utteranceText = norm.utteranceText;
                }
            }
            if (rawButton.contains(QLatin1String("effectsBaked"))) {
                dest->effectsBaked = norm.effectsBaked;
            }
            if (rawButton.contains(QLatin1String("col")) || rawButton.contains(QLatin1String("x"))) {
                dest->col = norm.col;
            }
            if (rawButton.contains(QLatin1String("row")) || rawButton.contains(QLatin1String("y"))) {
                dest->row = norm.row;
            }
            if (rawButton.contains(QLatin1String("colSpan"))) {
                dest->colSpan = norm.colSpan;
            }
            if (rawButton.contains(QLatin1String("rowSpan"))) {
                dest->rowSpan = norm.rowSpan;
            }
            if (rawButton.contains(QLatin1String("audioData"))) {
                const QString data = rawButton.value(QStringLiteral("audioData")).toString();
                if (data.startsWith(QLatin1String("data:"))) {
                    rememberAudio(dest->id, rawButton);
                } else {
                    dest->clipId.clear();
                    dest->effectsBaked = false;
                }
            }
        }
        finishTopic(existing);
        keptOne = true;
    }

    if (next.isEmpty() || (mode == WorkspaceMode::Replace && !keptOne)) {
        return fail(QStringLiteral("No valid topics in file"));
    }

    const QString preferred = root.value(QStringLiteral("activeTopicId")).toString();
    QString active = next.first().id;
    for (const SoundboardTopic& topic : next) {
        if (topic.id == preferred) {
            active = topic.id;
            break;
        }
    }
    m_topics = std::move(next);
    m_activeTopicId = active;
    evictUnreferencedClips();
    for (auto it = audioById.constBegin(); it != audioById.constEnd(); ++it) {
        SoundboardButton* button = findButton(it.key());
        if (!button) {
            continue;
        }
        const QString data = it.value();
        const int comma = data.indexOf(QLatin1Char(','));
        if (comma < 0) {
            continue;
        }
        const QByteArray bytes = QByteArray::fromBase64(data.mid(comma + 1).toLatin1());
        if (bytes.isEmpty() || bytes.size() > kMaxClipBytes) {
            continue;
        }
        const QString header = data.left(comma).toLower();
        const bool wav = header.contains(QLatin1String("audio/wav"))
                         || header.contains(QLatin1String("audio/x-wav"));
        QTemporaryFile tmp(QDir::temp().filePath(QStringLiteral("gazer-import-XXXXXX.")
                                                 + (wav ? QStringLiteral("wav")
                                                        : QStringLiteral("mp3"))));
        tmp.setAutoRemove(true);
        if (!tmp.open() || tmp.write(bytes) != bytes.size() || !tmp.flush()) {
            continue;
        }
        const QString clipId = importClip(tmp.fileName(), nullptr, nullptr);
        if (clipId.isEmpty()) {
            continue;
        }
        if (SoundboardButton* live = findButton(it.key())) {
            live->clipId = clipId;
        }
    }
    if (imported) {
        *imported = parsed;
    }
    return true;
}

void SoundboardStore::restoreTopics(QVector<SoundboardTopic> topics, const QString& activeId)
{
    m_topics = std::move(topics);
    m_activeTopicId = activeId;
    if (!findTopic(m_activeTopicId) && !m_topics.isEmpty()) {
        m_activeTopicId = m_topics.first().id;
    }
}

} // namespace gazer
