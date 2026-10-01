#pragma once

#include <QObject>

namespace gazer {

/// Default-playback-device master volume (WASAPI). GUI thread.
class SystemVolume final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMin = 0;
    static constexpr int kMax = 100;
    static constexpr int kStep = 10;

    explicit SystemVolume(QObject* parent = nullptr);
    ~SystemVolume() override;

    [[nodiscard]] int percent() const;
    [[nodiscard]] bool muted() const;
    /// 0–100 scalar. Unmutes. True when the level is already there or the
    /// endpoint accepted the write. `changed` fires only when mute or level changes.
    bool setPercent(int percent);
    bool setMuted(bool on);
    /// ±10 (callers pass ±1). Unmutes on increase.
    bool nudge(int dir);

    [[nodiscard]] static int clampPercent(int percent)
    {
        return qBound(kMin, percent, kMax);
    }

signals:
    void changed();

private slots:
    void onEndpointChanged();

private:
    friend class SystemVolumeNotify;
    bool ensureEndpoint();
    void releaseEndpoint();

    void* m_enumerator = nullptr; // IMMDeviceEnumerator*
    void* m_endpoint = nullptr;   // IAudioEndpointVolume*
    void* m_notify = nullptr;     // SystemVolumeNotify*
    bool m_setting = false;
};

} // namespace gazer
