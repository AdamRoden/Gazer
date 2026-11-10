#include "assist/PcmFx.h"

#include <QtGlobal>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace gazer::PcmFx {
namespace {

constexpr float kPi = 3.14159265358979323846f;

double clampd(double v, double lo, double hi, double fallback)
{
    if (!std::isfinite(v)) {
        v = fallback;
    }
    return std::clamp(v, lo, hi);
}

float sampleAt(const QVector<float>& data, double pos)
{
    if (data.isEmpty()) {
        return 0.f;
    }
    if (pos <= 0.0) {
        return data.first();
    }
    if (pos >= data.size() - 1) {
        return data.last();
    }
    const int i0 = int(std::floor(pos));
    const float frac = float(pos - i0);
    return data.at(i0) * (1.f - frac) + data.at(i0 + 1) * frac;
}

QVector<float> resample(const QVector<float>& input, int newLen)
{
    const int target = qMax(1, newLen);
    if (input.isEmpty()) {
        return QVector<float>(target, 0.f);
    }
    if (input.size() == target) {
        return input;
    }
    QVector<float> out(target);
    if (target == 1) {
        out[0] = input.first();
        return out;
    }
    const double ratio = double(input.size() - 1) / double(target - 1);
    for (int i = 0; i < target; ++i) {
        out[i] = sampleAt(input, i * ratio);
    }
    return out;
}

QVector<float> hann(int n)
{
    QVector<float> w(qMax(0, n), 0.f);
    if (n <= 1) {
        if (n == 1) {
            w[0] = 1.f;
        }
        return w;
    }
    for (int i = 0; i < n; ++i) {
        w[i] = 0.5f * (1.f - std::cos((2.f * kPi * float(i)) / float(n - 1)));
    }
    return w;
}

QVector<float> wsola(const QVector<float>& input, float stretch, int sampleRate)
{
    if (input.size() < 2) {
        return input;
    }
    const float ratio = float(std::clamp(double(stretch), 0.25, 4.0));
    if (std::abs(ratio - 1.f) < 1e-4f) {
        return input;
    }
    const int sr = sampleRate > 0 ? sampleRate : 44100;
    int winSize = int(std::lround(sr * 0.028));
    if (winSize < 64) {
        winSize = 64;
    }
    if (winSize % 2) {
        ++winSize;
    }
    const int analysisHop = qMax(1, int(std::lround(winSize * 0.25)));
    const int synthesisHop = qMax(1, int(std::lround(analysisHop * ratio)));
    const int searchRadius = qMax(0, int(std::lround(winSize * 0.35)));
    const QVector<float> window = hann(winSize);
    const int outLen = qMax(1, int(std::lround(input.size() * ratio)));
    QVector<float> output(outLen, 0.f);
    QVector<float> norm(outLen, 0.f);

    const auto writeFrame = [&](int srcStart, int dstStart) {
        for (int i = 0; i < winSize; ++i) {
            const int di = dstStart + i;
            if (di < 0 || di >= outLen) {
                continue;
            }
            const int si = srcStart + i;
            const float s = (si >= 0 && si < input.size()) ? input.at(si) : 0.f;
            const float w = window.at(i);
            output[di] += s * w;
            norm[di] += w;
        }
    };

    writeFrame(0, 0);
    int inPos = analysisHop;
    int outPos = synthesisHop;
    while (outPos < outLen + winSize && inPos < input.size() + searchRadius) {
        int bestDelta = 0;
        float bestScore = -1e30f;
        const int overlap = qMin(winSize, synthesisHop * 2);
        for (int delta = -searchRadius; delta <= searchRadius; ++delta) {
            const int candidate = inPos + delta;
            if (candidate < 0 || candidate + winSize > input.size() + winSize) {
                continue;
            }
            float score = 0.f;
            const int n = qMin(overlap, winSize);
            for (int i = 0; i < n; ++i) {
                const int di = outPos + i;
                const int si = candidate + i;
                if (di < 0 || di >= outLen) {
                    continue;
                }
                const float existing = norm.at(di) > 1e-8f ? output.at(di) / norm.at(di) : 0.f;
                const float incoming = (si >= 0 && si < input.size()) ? input.at(si) : 0.f;
                score += existing * incoming;
            }
            if (score > bestScore) {
                bestScore = score;
                bestDelta = delta;
            }
        }
        writeFrame(inPos + bestDelta, outPos);
        inPos += analysisHop;
        outPos += synthesisHop;
        if (inPos > input.size() + searchRadius && outPos > outLen) {
            break;
        }
    }
    for (int i = 0; i < outLen; ++i) {
        if (norm.at(i) > 1e-8f) {
            output[i] /= norm.at(i);
        }
    }
    return output;
}

QVector<float> toFloat(const QByteArray& pcm)
{
    const int bytes = pcm.size() - (pcm.size() & 1);
    const int n = bytes / 2;
    QVector<float> out(n);
    const auto* src = reinterpret_cast<const qint16*>(pcm.constData());
    for (int i = 0; i < n; ++i) {
        out[i] = float(src[i]) / 32768.f;
    }
    return out;
}

QByteArray toPcm(const QVector<float>& samples)
{
    QByteArray out(samples.size() * 2, '\0');
    auto* dst = reinterpret_cast<qint16*>(out.data());
    for (int i = 0; i < samples.size(); ++i) {
        const int v = int(std::lround(double(samples.at(i)) * 32767.0));
        dst[i] = qint16(std::clamp(v, -32768, 32767));
    }
    return out;
}

} // namespace

bool isIdentity(double speed, double pitch)
{
    const double s = clampd(speed, 0.25, 4.0, 1.0);
    const double p = clampd(pitch, 0.5, 2.0, 1.0);
    return std::abs(s - 1.0) < 1e-6 && std::abs(p - 1.0) < 1e-6;
}

QByteArray apply(const QByteArray& pcm, int sampleRate, double speed, double pitch)
{
    const int even = pcm.size() - (pcm.size() & 1);
    if (even < 2) {
        return {};
    }
    const double s = clampd(speed, 0.25, 4.0, 1.0);
    const double p = clampd(pitch, 0.5, 2.0, 1.0);
    if (std::abs(s - 1.0) < 1e-6 && std::abs(p - 1.0) < 1e-6) {
        return even == pcm.size() ? pcm : pcm.left(even);
    }
    const QVector<float> original = toFloat(pcm.left(even));
    QVector<float> buf = original;
    if (std::abs(p - 1.0) >= 1e-6) {
        buf = resample(original, qMax(1, int(std::lround(original.size() / p))));
    }
    const int targetLen = qMax(1, int(std::lround(original.size() / s)));
    if (buf.size() != targetLen) {
        buf = wsola(buf, float(targetLen) / float(qMax(1, buf.size())), sampleRate);
    }
    return toPcm(buf);
}

} // namespace gazer::PcmFx
