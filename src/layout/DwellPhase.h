#pragma once

#include <QHash>
#include <QString>
#include <QtGlobal>
#include <optional>

namespace gazer {

/// Per-cell dwell phases: activation arms/advances; leave commits the current
/// phase. Index is 0-based and latches on the last phase until leave.
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
        m_index[key] = qMin(m_index.value(key) + 1, phaseCount - 1);
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

    [[nodiscard]] bool armed(const QString& key) const { return m_index.contains(key); }

private:
    QHash<QString, int> m_index;
};

} // namespace gazer
