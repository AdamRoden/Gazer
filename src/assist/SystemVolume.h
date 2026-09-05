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
    /// 0–100 scalar. Unmutes. True if the endpoint accepted the write.
    bool setPercent(int percent);
    bool setMuted(bool on);
    /// ±10 (callers pass ±1). Unmutes on increase.
    bool nudge(int dir);

    [[nodiscard]] static int clampPercent(int percent);

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
