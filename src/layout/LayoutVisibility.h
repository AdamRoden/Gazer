#pragma once

#include "layout/LayoutTypes.h"

#include <QString>
#include <QVariantMap>

namespace gazer {

/// Evaluate `visibleWhen`: empty → true; `ident` / `!ident` against props.
/// Unknown syntax fails open so boards stay usable.
[[nodiscard]] inline bool evalVisibleWhen(const QString& expr, const QVariantMap& props)
{
    const QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    const bool negated = trimmed.startsWith(QLatin1Char('!'));
    const QString key = (negated ? trimmed.mid(1) : trimmed).trimmed();
    if (key.isEmpty()) {
        return true;
    }
    for (const QChar c : key) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_')) {
            return true;
        }
    }
    const bool truthy = props.value(key).toBool();
    return negated ? !truthy : truthy;
}

[[nodiscard]] inline bool itemIsShown(const LayoutItem& item, const QVariantMap& props)
{
    if (!item.visible) {
        return false;
    }
    return evalVisibleWhen(item.visibleWhen, props);
}

} // namespace gazer
