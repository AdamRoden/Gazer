#include "assist/ElevenRequest.h"

#include "assist/PcmWav.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace ElevenRequest {
namespace {

const QRegularExpression& tagRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\[[^\]]*\])"));
    return re;
}

double clampOrDefault(double v, double lo, double hi, double fallback)
{
    if (!std::isfinite(v)) {
        v = fallback;
    }
    return std::clamp(v, lo, hi);
}

bool isLegacyEleven(const QString& s)
{
    return s == kModelTurbo || s == QLatin1String("eleven_v3")
           || s == QLatin1String("eleven_flash_v2_5") || s == QLatin1String("eleven_flash_v2")
           || s == QLatin1String("eleven_multilingual_v2") || s == QLatin1String("eleven_v4");
}

QByteArray jsonBytes(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

} // namespace

QString normalizeModelId(QStringView id)
{
    const QString s = id.toString();
    if (s == kModelSapi) {
        return s;
    }
    if (isLegacyEleven(s)) {
        return kModelTurbo.toString();
    }
    return kModelSapi.toString();
}

bool isElevenModel(QStringView id)
{
    return normalizeModelId(id) == kModelTurbo;
}

bool phraseHasInlineTags(QStringView text)
{
    return tagRe().matchView(text).hasMatch();
}

QString stripInlineTags(QStringView text)
{
    QString s(text);
    s.replace(tagRe(), QStringLiteral(" "));
    return s.simplified();
}

bool hasNonTagSpeechContent(QStringView text)
{
    return !stripInlineTags(text).isEmpty();
}

QString normalizeStandingTag(QStringView raw)
{
    QString t = raw.toString().trimmed();
    if (t.size() >= 2 && t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']'))) {
        t = t.mid(1, t.size() - 2).trimmed();
    }
    t.remove(QLatin1Char('['));
    t.remove(QLatin1Char(']'));
    return t.simplified();
}

QString applyStandingTag(QStringView phrase, QStringView tag)
{
    const QString clean = normalizeStandingTag(tag);
    const QString text = phrase.toString();
    if (clean.isEmpty()) {
        return text;
    }
    const QString wrapped = QLatin1Char('[') + clean + QLatin1Char(']');
    int lead = 0;
    while (lead < text.size() && text.at(lead).isSpace()) {
        ++lead;
    }
    const QStringView head = QStringView(text).mid(lead);
    if (head.size() >= wrapped.size()
        && head.left(wrapped.size()).compare(QStringView(wrapped), Qt::CaseInsensitive) == 0) {
        return text;
    }
    return wrapped + QLatin1Char(' ') + text;
}

Prepared prepareSpeakRequest(QStringView phrase, QStringView selectedModel, double speed)
{
    const QString phraseStr(phrase);
    const QString modelId = normalizeModelId(selectedModel);
    Prepared out;
    out.modelId = modelId;
    out.text = modelId == kModelTurbo ? phraseStr : stripInlineTags(phraseStr);
    out.localSpeed = clampOrDefault(speed, kDesiredSpeedMin, kDesiredSpeedMax, 1.0);
    return out;
}

QString dialogueStreamUrl()
{
    QUrl url(QStringLiteral("wss://api.elevenlabs.io/v1/text-to-dialogue/stream-input"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("model_id"), kModelTurbo.toString());
    query.addQueryItem(QStringLiteral("output_format"),
                       QStringLiteral("pcm_%1").arg(PcmWav::kSampleRate));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

QByteArray registerVoicesMessage(QStringView voiceId)
{
    QJsonObject obj;
    QJsonArray voices;
    voices.append(voiceId.toString());
    obj.insert(QStringLiteral("voices"), voices);
    return jsonBytes(obj);
}

QByteArray speakInputsMessage(QStringView text, QStringView voiceId)
{
    QJsonObject line;
    line.insert(QStringLiteral("text"), text.toString());
    line.insert(QStringLiteral("voice_id"), voiceId.toString());
    line.insert(QStringLiteral("new_turn"), true);
    QJsonArray inputs;
    inputs.append(line);
    QJsonObject obj;
    obj.insert(QStringLiteral("inputs"), inputs);
    return jsonBytes(obj);
}

QByteArray closeSocketMessage()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("close_socket"), true);
    return jsonBytes(obj);
}

QByteArray keepAliveMessage()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("keep_alive"), true);
    return jsonBytes(obj);
}

} // namespace ElevenRequest
} // namespace gazer
