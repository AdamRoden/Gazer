#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringView>
#include <optional>

namespace gazer {
namespace ElevenRequest {

inline constexpr double kDesiredSpeedMin = 0.25;
inline constexpr double kDesiredSpeedMax = 4.0;
inline constexpr double kApiSpeedMin = 0.7;
inline constexpr double kApiSpeedMax = 1.2;
inline constexpr double kPitchMin = 0.5;
inline constexpr double kPitchMax = 2.0;
inline constexpr int kSpeakTimeoutMs = 25000;
inline constexpr int kVoicesTimeoutMs = 15000;

inline constexpr QStringView kModelSapi = u"sapi";
inline constexpr QStringView kModelV3 = u"eleven_v3";
inline constexpr QStringView kModelFlash = u"eleven_flash_v2_5";
inline constexpr QStringView kVoicesUrl = u"https://api.elevenlabs.io/v1/voices";

struct SpeedSplit {
    std::optional<double> apiSpeed; // nullopt on v3
    double localSpeed = 1.0;
};

struct Prepared {
    QString modelId;   // sapi, eleven_v3, or eleven_flash_v2_5
    QString text;      // tags kept only for v3
    QJsonObject body;  // { text, model_id, voice_settings? }
    double localSpeed = 1.0;
    double pitch = 1.0;
};

/// Canonical model id. Aliases: eleven_flash_v2 / eleven_multilingual_v2 → flash.
/// Unknown (including Voice browser_tts / piper_tts) → sapi.
[[nodiscard]] QString normalizeModelId(QStringView id);
[[nodiscard]] bool isElevenModel(QStringView id);

/// True if text contains Eleven-style [tag] directives.
[[nodiscard]] bool phraseHasInlineTags(QStringView text);
/// Replace [bracket] segments with a space, collapse whitespace, trim.
[[nodiscard]] QString stripInlineTags(QStringView text);
[[nodiscard]] bool hasNonTagSpeechContent(QStringView text);

/// Split desired speed into API portion (0.7–1.2) + local remainder.
/// v3 does not support API speed — all speed is local.
[[nodiscard]] SpeedSplit splitSpeed(double desired, QStringView modelId);

/// Build TTS request pieces. Tags force v3; non-v3 text has brackets stripped.
[[nodiscard]] Prepared prepareSpeakRequest(QStringView phrase, QStringView selectedModel,
                                           double speed, double pitch);

[[nodiscard]] QString speakUrl(QStringView voiceId);

} // namespace ElevenRequest
} // namespace gazer
