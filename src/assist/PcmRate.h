#pragma once

#include <QByteArray>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace gazer::PcmRate {

/// Streaming int16 resampler. `speed` > 1 shortens playback (same pitch shift as
/// QMediaPlayer::setPlaybackRate). Call `flush` once when the utterance ends.
class Resampler {
public:
    void reset()
    {
        m_hold.clear();
        m_pos = 0;
        m_hasHalf = false;
        m_half = 0;
    }

    [[nodiscard]] QByteArray process(const QByteArray& pcm, double speed)
    {
        appendPcm(pcm);
        return pull(speed, false);
    }

    [[nodiscard]] QByteArray flush(double speed)
    {
        m_hasHalf = false;
        QByteArray out = pull(speed, true);
        reset();
        return out;
    }

private:
    void appendPcm(const QByteArray& pcm)
    {
        QByteArray even;
        even.reserve(pcm.size() + 1);
        if (m_hasHalf) {
            even.append(m_half);
            m_hasHalf = false;
        }
        even.append(pcm);
        if (even.size() & 1) {
            m_half = even.back();
            m_hasHalf = true;
            even.chop(1);
        }
        const auto* samples = reinterpret_cast<const qint16*>(even.constData());
        const int n = even.size() / 2;
        for (int i = 0; i < n; ++i) {
            m_hold.append(samples[i]);
        }
    }

    [[nodiscard]] QByteArray pull(double speed, bool ending)
    {
        QByteArray out;
        if (m_hold.isEmpty()) {
            return out;
        }
        double step = speed;
        if (!std::isfinite(step)) {
            step = 1.0;
        }
        step = std::clamp(step, 0.25, 4.0);
        const int guard = m_hold.size() * 8 + 8;
        int emitted = 0;
        while (emitted < guard) {
            const bool more = ending ? (m_pos < double(m_hold.size()))
                                     : (m_pos + 1.0 < double(m_hold.size()));
            if (!more) {
                break;
            }
            const int last = m_hold.size() - 1;
            const int i0 = std::clamp(int(std::floor(m_pos)), 0, last);
            const int i1 = std::clamp(i0 + 1, 0, last);
            const double frac = std::clamp(m_pos - double(i0), 0.0, 1.0);
            const double s = double(m_hold.at(i0)) * (1.0 - frac) + double(m_hold.at(i1)) * frac;
            const int v = int(std::lround(s));
            const qint16 sample = qint16(std::clamp(v, -32768, 32767));
            out.append(reinterpret_cast<const char*>(&sample), 2);
            m_pos += step;
            ++emitted;
        }
        const int drop = int(std::floor(m_pos));
        if (drop > 0) {
            const int n = std::min(drop, int(m_hold.size()));
            m_hold.remove(0, n);
            m_pos -= n;
            if (m_pos < 0) {
                m_pos = 0;
            }
        }
        return out;
    }

    QVector<qint16> m_hold;
    double m_pos = 0;
    char m_half = 0;
    bool m_hasHalf = false;
};

} // namespace gazer::PcmRate
