#pragma once

#include <QByteArray>

namespace gazer::PcmFx {

/// True when speed and pitch would leave the ElevenLabs PCM unchanged.
[[nodiscard]] bool isIdentity(double speed, double pitch);

/// Pitch is a resample. Speed is a pitch-preserving time-stretch back toward
/// `sampleCount / speed` of the original. Identity returns the even-length PCM.
[[nodiscard]] QByteArray apply(const QByteArray& pcm, int sampleRate, double speed, double pitch);

} // namespace gazer::PcmFx
