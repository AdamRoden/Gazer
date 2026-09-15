#pragma once

#include <QHash>
#include <QString>
#include <optional>

namespace gazer {

/// Per-cell dwell phases: first activation enters phase 0; each later
/// activation advances, wrapping last → first. Leave after blink grace
/// commits the current phase.
struct DwellPhaseBank {
    void onActivated(const QString& key, int phaseCount)
    {
        if (key.isEmpty() || phaseCount <= 0) {
            return;
        }
        if (!m_index.contains(key)) {
            m_index.insert(key, 0);
            return;
        }
        m_index[key] = (m_index.value(key) + 1) % phaseCount;
    }

    [[nodiscard]] std::optional<int> current(const QString& key) const
    {
        const auto it = m_index.constFind(key);
        if (it == m_index.cend()) {
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<int> takeCommit(const QString& key)
    {
        const auto it = m_index.find(key);
        if (it == m_index.end()) {
            return std::nullopt;
        }
        const int i = it.value();
        m_index.erase(it);
        return i;
    }

    void clear() { m_index.clear(); }

private:
    QHash<QString, int> m_index;
};

} // namespace gazer
