#pragma once

#include "layout/PageTypes.h"

#include <QByteArray>
#include <QString>

namespace gazer {

class PageWriter {
public:
    [[nodiscard]] static QByteArray toBytes(const PageDocument& doc);
    [[nodiscard]] static bool saveToFile(const PageDocument& doc, const QString& path,
                                         QString* error = nullptr);
};

} // namespace gazer
