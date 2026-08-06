#pragma once

#include <QPointF>
#include <QString>

namespace gazer {

/// Lightweight snapshot for edge-switcher UI (no LayoutInstance dependency).
struct InstanceTarget {
    QString instanceId;
    QString label;
    QPointF centerScreen;
};

} // namespace gazer
