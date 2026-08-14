#pragma once

#include <QElapsedTimer>
#include <QtGlobal>

namespace gazer {

/// Hold through a brief invalid / empty hit so dwell does not rewind.
struct InvalidGazeGrace {
    enum class Result { Holding, Expired };

    int graceMs = 180;

    Result onInvalid()
    {
        if (!m_holding) {
            m_holding = true;
            m_clock.start();
            return Result::Holding;
        }
        return m_clock.elapsed() < graceMs ? Result::Holding : Result::Expired;
    }

    void onValid() { m_holding = false; }
    void reset() { m_holding = false; }
    [[nodiscard]] bool holding() const { return m_holding; }

private:
    bool m_holding = false;
    QElapsedTimer m_clock;
};

} // namespace gazer
