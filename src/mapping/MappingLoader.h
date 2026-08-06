#pragma once

#include "mapping/MappingTypes.h"

#include <QString>

namespace gazer {

class MappingLoader {
public:
    [[nodiscard]] static bool loadFromFile(const QString& path, MappingProfile& out,
                                           QString* error = nullptr);
    [[nodiscard]] static bool loadFromJson(const QByteArray& json, MappingProfile& out,
                                           QString* error = nullptr);
};

} // namespace gazer
