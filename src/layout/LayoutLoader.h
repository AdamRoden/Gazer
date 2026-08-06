#pragma once

#include "layout/LayoutTypes.h"

#include <QString>

namespace gazer {

/// Parse layout JSON (schema v1) from file or string.
class LayoutLoader {
public:
    [[nodiscard]] static bool loadFromFile(const QString& path,
                                           LayoutDocument& out,
                                           QString* error = nullptr);

    [[nodiscard]] static bool loadFromJson(const QByteArray& json,
                                           LayoutDocument& out,
                                           QString* error = nullptr);
};

} // namespace gazer
