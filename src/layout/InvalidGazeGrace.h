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

} // namespace gazer
