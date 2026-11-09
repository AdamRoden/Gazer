#pragma once

#include <QString>

namespace gazer {

/// Caption payload for `role="scrollbar"`: `offset,visible,total`.
[[nodiscard]] inline QString scrollCaption(int offset, int visible, int total)
{
    return QStringLiteral("%1,%2,%3").arg(offset).arg(visible).arg(total);
}

} // namespace gazer
