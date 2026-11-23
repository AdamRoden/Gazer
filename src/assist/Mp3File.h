#pragma once

#include <QString>

namespace gazer {

/// Write `dest` as an MP3. An MP3 source is copied. Any other readable clip is encoded.
/// Replaces `dest` only after the new file is complete.
[[nodiscard]] bool writeMp3File(const QString& source, const QString& dest, QString* error = nullptr);

} // namespace gazer
