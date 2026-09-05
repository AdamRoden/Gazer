#pragma once

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace gazer::AudioGain {

constexpr double kMin = 1.0;
constexpr double kMax = 5.0;

[[nodiscard]] inline double clamp(double gain)
{
    if (!std::isfinite(gain)) {
        return 1.0;
    }
    return std::clamp(gain, kMin, kMax);
}

[[nodiscard]] inline bool needsDecode(double gain)
{
    return clamp(gain) > 1.0 + 1e-6;
}

/// Scale packed native-endian int16 PCM in place. Odd trailing byte is ignored.
inline void scaleInt16(char* data, int bytes, double gain)
{
    if (!data || bytes < 2) {
        return;
    }
    if (!std::isfinite(gain) || std::abs(gain - 1.0) < 1e-6) {
        return;
    }
    gain = std::clamp(gain, 0.05, 10.0);
    auto* samples = reinterpret_cast<qint16*>(data);
    const int n = bytes / 2;
    for (int i = 0; i < n; ++i) {
        const int v = int(std::lround(double(samples[i]) * gain));
        samples[i] = qint16(std::clamp(v, -32768, 32767));
    }
}

} // namespace gazer::AudioGain
