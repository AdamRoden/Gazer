#pragma once

#include "layout/LayoutInstanceManager.h"

#include <QString>

namespace gazer {

/// Single product policy for layout "loadLayout" / script gazer.loadLayout.
/// Delegates to LayoutInstanceManager::applyLoadLayout (one implementation).
inline bool applyLoadLayout(LayoutInstanceManager& sessions, const QString& sourceInstanceId,
                            const QString& layoutId, QString* error = nullptr)
{
    return sessions.applyLoadLayout(sourceInstanceId, layoutId, error);
}

} // namespace gazer
