#pragma once

#include <QObject>
#include <QString>

namespace gazer {

/// Master volume for the Speak playback device (WASAPI `IAudioEndpointVolume`).
/// The device comes from `AudioOutput::resolveDevice` (`speechOutputId`).
/// An empty key is the Windows default. A missing device follows that default
/// until it returns. GUI thread.
class SystemVolume final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMin = 0;
    static constexpr int kMax = 100;
    static constexpr int kStep = 10;

    explicit SystemVolume(QObject* parent = nullptr);
    ~SystemVolume() override;

    /// Bind `speechOutputId`. Empty selects the Windows default. True when an
    /// endpoint is open. `changed` fires when the bound endpoint changes.
    bool setDeviceKey(const QString& key);
    /// Id of the endpoint currently controlled. Empty when none is open.
    [[nodiscard]] QString boundEndpointId() const { return m_boundEndpointId; }

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
    bool ensureEnumerator();
    /// Open an endpoint when none is open. An open endpoint stays put.
    bool ensureEndpoint();
    /// Open the resolved device, swapping only when its id differs.
    bool retarget();
    /// `retarget` and emit `changed` when the bound endpoint differs.
    bool bindCurrent();
    void releaseEndpoint();
    void releaseEnumerator();
    void noteFollow(bool fellBack);

    QString m_deviceKey;
    QString m_boundEndpointId;
    bool m_fallbackLogged = false;
    bool m_comInited = false;
    void* m_enumerator = nullptr; // IMMDeviceEnumerator*
    void* m_endpoint = nullptr;   // IAudioEndpointVolume*
    void* m_notify = nullptr;     // SystemVolumeNotify*
    bool m_setting = false;
};

} // namespace gazer
