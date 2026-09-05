#include "assist/ElevenRequest.h"

#include <QRegularExpression>
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

} // namespace

QString normalizeModelId(QStringView id)
{
    const QString s = id.toString();
    if (s == kModelSapi || s == kModelV3 || s == kModelFlash) {
        return s;
    }
    if (s == QLatin1String("eleven_flash_v2")
        || s == QLatin1String("eleven_multilingual_v2")) {
        return kModelFlash.toString();
    }
    return kModelSapi.toString();
}

bool isElevenModel(QStringView id)
{
    const QString m = normalizeModelId(id);
    return m == kModelV3 || m == kModelFlash;
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

SpeedSplit splitSpeed(double desired, QStringView modelId)
{
    const double speed = clampOrDefault(desired, kDesiredSpeedMin, kDesiredSpeedMax, 1.0);
    const QString model = normalizeModelId(modelId);
    if (model == kModelV3) {
        return {std::nullopt, speed};
    }
    const double api = std::clamp(speed, kApiSpeedMin, kApiSpeedMax);
    return {api, speed / api};
}

Prepared prepareSpeakRequest(QStringView phrase, QStringView selectedModel, double speed,
                             double pitch)
{
    const QString phraseStr(phrase);
    const double spd = clampOrDefault(speed, kDesiredSpeedMin, kDesiredSpeedMax, 1.0);
    const double pit = clampOrDefault(pitch, kPitchMin, kPitchMax, 1.0);
    const bool hasTags = phraseHasInlineTags(phraseStr);
    const QString selected = normalizeModelId(selectedModel);
    const QString modelId = hasTags ? kModelV3.toString() : selected;
    const QString text = modelId == kModelV3 ? phraseStr : stripInlineTags(phraseStr);
    const SpeedSplit split = splitSpeed(spd, modelId);

    QJsonObject body;
    body.insert(QStringLiteral("text"), text);
    body.insert(QStringLiteral("model_id"), modelId);
    if (split.apiSpeed) {
        QJsonObject vs;
        vs.insert(QStringLiteral("speed"), *split.apiSpeed);
        body.insert(QStringLiteral("voice_settings"), vs);
    }

    Prepared out;
    out.modelId = modelId;
    out.text = text;
    out.body = body;
    out.localSpeed = split.localSpeed;
    out.pitch = pit;
    return out;
}

QString speakUrl(QStringView voiceId)
{
    return QStringLiteral("https://api.elevenlabs.io/v1/text-to-speech/%1").arg(voiceId);
}

} // namespace ElevenRequest
} // namespace gazer
