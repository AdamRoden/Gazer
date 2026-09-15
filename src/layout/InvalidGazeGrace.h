#pragma once

#include <QtGlobal>

namespace gazer {

/// Hold through a brief invalid / empty hit so dwell does not rewind.
/// Duration is measured in the same timestamp units as gaze samples.
struct InvalidGazeGrace {
    enum class Result { Holding, Expired };

    int graceMs = 180;

    Result onInvalid(qint64 timestampMs)
    {
        if (graceMs <= 0) {
            return Result::Expired;
        }
        if (!m_holding) {
            m_holding = true;
            m_startMs = timestampMs;
            return Result::Holding;
        }
        return (timestampMs - m_startMs) < graceMs ? Result::Holding : Result::Expired;
    }

    void onValid() { m_holding = false; }
    void reset() { m_holding = false; }
    [[nodiscard]] bool holding() const { return m_holding; }

private:
    bool m_holding = false;
    qint64 m_startMs = 0;
};

/// Block a new dwell until @p holdMs after the first sample that checks it.
struct StartHold {
    int holdMs = 0;

    void arm(int ms)
    {
        holdMs = qMax(0, ms);
        m_started = false;
        m_startMs = 0;
    }

    [[nodiscard]] bool blocking(qint64 timestampMs)
    {
        if (holdMs <= 0) {
            return false;
        }
        if (!m_started) {
            m_started = true;
            m_startMs = timestampMs;
        }
        if (timestampMs - m_startMs < holdMs) {
            return true;
        }
        reset();
        return false;
    }

    void reset()
    {
        holdMs = 0;
        m_started = false;
        m_startMs = 0;
    }

private:
    bool m_started = false;
    qint64 m_startMs = 0;
};

} // namespace gazer
