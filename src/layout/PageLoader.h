#pragma once

#include "layout/PageTypes.h"

#include <QByteArray>
#include <QString>

namespace gazer {

class PageLoader {
public:
    [[nodiscard]] static bool loadFromFile(const QString& path, PageDocument& out,
                                           QString* error = nullptr);
    [[nodiscard]] static bool loadFromXml(const QByteArray& xml, PageDocument& out,
                                          QString* error = nullptr);
};

} // namespace gazer
