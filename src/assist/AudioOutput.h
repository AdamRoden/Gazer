#pragma once

#include <QAudioDevice>
#include <QByteArray>
#include <QString>
#include <QVector>

namespace gazer {
namespace AudioOutput {

/// Stable key for a playback endpoint. Empty means the Windows default.
[[nodiscard]] QString deviceKey(const QByteArray& id);
/// Inverse of `deviceKey`. Empty when `key` is empty or not a key we wrote.
[[nodiscard]] QByteArray idFromKey(const QString& key);

struct PlaybackDevice {
    QString key;
    QString name;
    bool isDefault = false;
};

[[nodiscard]] QVector<PlaybackDevice> playbackDevices();
[[nodiscard]] QString defaultPlaybackName();
/// Live label for a saved key. Empty when that device is not connected.
[[nodiscard]] QString playbackName(const QString& key);
/// `fellBack` is set when `key` names a device that is not connected.
[[nodiscard]] QAudioDevice resolveDevice(const QString& key, bool* fellBack = nullptr);

} // namespace AudioOutput
} // namespace gazer
