#pragma once

#include "layout/LayoutTypes.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace gazer {

/// Serialize a LayoutDocument to schema-v1 JSON (inverse of LayoutLoader).
class LayoutWriter {
public:
    [[nodiscard]] static QJsonObject toJson(const LayoutDocument& doc);
    [[nodiscard]] static QByteArray toBytes(const LayoutDocument& doc);
    [[nodiscard]] static bool saveToFile(const LayoutDocument& doc, const QString& path,
                                         QString* error = nullptr);
};

} // namespace gazer
