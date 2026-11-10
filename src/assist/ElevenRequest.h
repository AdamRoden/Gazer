#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace gazer {
namespace ElevenRequest {

inline constexpr double kDesiredSpeedMin = 0.25;
inline constexpr double kDesiredSpeedMax = 4.0;
inline constexpr int kSpeakTimeoutMs = 25000;
inline constexpr int kVoicesTimeoutMs = 15000;

inline constexpr QStringView kModelSapi = u"sapi";
inline constexpr QStringView kModelTurbo = u"eleven_v4_turbo";
inline constexpr QStringView kVoicesUrl = u"https://api.elevenlabs.io/v2/voices";

struct Prepared {
    QString modelId; // sapi or eleven_v4_turbo
    QString text;    // audio tags kept for v4 Turbo
    double localSpeed = 1.0;
};

/// Canonical model id. Older Eleven ids (v3, Flash, Multilingual v2, v4) → v4 Turbo.
/// Unknown (including Voice browser_tts / piper_tts) → sapi.
[[nodiscard]] QString normalizeModelId(QStringView id);
[[nodiscard]] bool isElevenModel(QStringView id);

/// True if text contains Eleven-style [tag] directives.
[[nodiscard]] bool phraseHasInlineTags(QStringView text);
/// Replace [bracket] segments with a space, collapse whitespace, trim.
[[nodiscard]] QString stripInlineTags(QStringView text);
[[nodiscard]] bool hasNonTagSpeechContent(QStringView text);

/// Standing voice tag: trim, drop one wrapping [pair], drop leftover brackets.
/// Empty means no standing tag. "none" is kept here; style names reject it.
[[nodiscard]] QString normalizeStandingTag(QStringView raw);
/// Put `[tag]` at the start of text sent to ElevenLabs. Leave the phrase
/// alone when it already starts with that tag (ignoring leading space).
[[nodiscard]] QString applyStandingTag(QStringView phrase, QStringView tag);

/// One request per breath when a standing style or an inline `[tag]` is present.
/// Untagged text stays one string. The standing style and any tags at the start
/// of the line are repeated on every breath. A tag written later stays on that
/// breath. Decimals, clock times, domains, and commas inside brackets stay whole.
[[nodiscard]] QStringList taggedSends(QStringView phrase, QStringView tag);

/// ElevenLabs key handoff. The approval page copies an `sk_` key. The code is
/// not exchanged. `challenge` and `state` are base64url.
[[nodiscard]] QString keyHandoffUrl(QStringView challenge, QStringView state);
[[nodiscard]] QString keyQuickstartUrl();

/// Build speak pieces. v4 Turbo keeps audio tags. Speed and pitch stay local.
/// SAPI text has brackets stripped. Desired speed is clamped to 0.25–4.
[[nodiscard]] Prepared prepareSpeakRequest(QStringView phrase, QStringView selectedModel,
                                           double speed);

/// Text-to-dialogue WebSocket. v4 Turbo is realtime on this route, one voice.
[[nodiscard]] QString dialogueStreamUrl();
[[nodiscard]] QByteArray registerVoicesMessage(QStringView voiceId);
[[nodiscard]] QByteArray speakInputsMessage(QStringView text, QStringView voiceId);
[[nodiscard]] QByteArray closeSocketMessage();
[[nodiscard]] QByteArray keepAliveMessage();

} // namespace ElevenRequest
} // namespace gazer
