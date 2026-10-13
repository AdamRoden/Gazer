#include "assist/AudioOutput.h"

#include <QMediaDevices>

namespace gazer {
namespace AudioOutput {

QString deviceKey(const QByteArray& id)
{
    if (id.isEmpty()) {
        return {};
    }
    return QString::fromLatin1(id.toBase64());
}

QByteArray idFromKey(const QString& key)
{
    const QString trimmed = key.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QByteArray raw = QByteArray::fromBase64(trimmed.toLatin1());
    if (raw.isEmpty() || deviceKey(raw) != trimmed) {
        return {};
    }
    return raw;
}

QAudioDevice resolveDevice(const QString& key, bool* fellBack)
{
    if (fellBack) {
        *fellBack = false;
    }
    const QString trimmed = key.trimmed();
    const QAudioDevice fallback = QMediaDevices::defaultAudioOutput();
    if (trimmed.isEmpty()) {
        return fallback;
    }
    const QByteArray id = idFromKey(trimmed);
    if (id.isEmpty()) {
        if (fellBack) {
            *fellBack = true;
        }
        return fallback;
    }
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice& dev : outputs) {
        if (!dev.isNull() && dev.id() == id) {
            return dev;
        }
    }
    if (fellBack) {
        *fellBack = true;
    }
    return fallback;
}

QVector<PlaybackDevice> playbackDevices()
{
    QVector<PlaybackDevice> out;
    const QAudioDevice def = QMediaDevices::defaultAudioOutput();
    const QByteArray defId = def.isNull() ? QByteArray() : def.id();
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    for (const QAudioDevice& dev : outputs) {
        if (dev.isNull() || dev.id().isEmpty()) {
            continue;
        }
        PlaybackDevice row;
        row.key = deviceKey(dev.id());
        if (row.key.isEmpty()) {
            continue;
        }
        bool dup = false;
        for (const PlaybackDevice& have : out) {
            if (have.key == row.key) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        row.name = dev.description().trimmed();
        if (row.name.isEmpty()) {
            row.name = QStringLiteral("Audio device");
        }
        row.isDefault = dev.isDefault() || (!defId.isEmpty() && dev.id() == defId);
        out.push_back(row);
    }
    return out;
}

QString defaultPlaybackName()
{
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    return dev.isNull() ? QString() : dev.description().trimmed();
}

QString playbackName(const QString& key)
{
    if (key.trimmed().isEmpty()) {
        return {};
    }
    bool fell = false;
    const QAudioDevice dev = resolveDevice(key, &fell);
    if (fell || dev.isNull()) {
        return {};
    }
    return dev.description().trimmed();
}

} // namespace AudioOutput
} // namespace gazer
