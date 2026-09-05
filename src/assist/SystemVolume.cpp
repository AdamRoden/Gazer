#include "assist/SystemVolume.h"

#include "utils/Log.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <mmdeviceapi.h>
#  include <endpointvolume.h>
#endif

namespace gazer {

int SystemVolume::clampPercent(int percent)
{
    return qBound(kMin, percent, kMax);
}

#ifdef Q_OS_WIN

class SystemVolumeNotify final : public IAudioEndpointVolumeCallback {
public:
    explicit SystemVolumeNotify(SystemVolume* owner)
        : m_owner(owner)
    {
    }

    void detach() { m_owner = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG n = InterlockedDecrement(&m_ref);
        if (n == 0) {
            delete this;
        }
        return n;
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override
    {
        if (m_owner) {
            QMetaObject::invokeMethod(m_owner, "onEndpointChanged", Qt::QueuedConnection);
        }
        return S_OK;
    }

private:
    SystemVolume* m_owner = nullptr;
    LONG m_ref = 1;
};

#endif

SystemVolume::SystemVolume(QObject* parent)
    : QObject(parent)
{
    (void)ensureEndpoint();
}

SystemVolume::~SystemVolume()
{
    releaseEndpoint();
}

bool SystemVolume::ensureEndpoint()
{
#ifdef Q_OS_WIN
    if (m_endpoint) {
        return true;
    }
    auto* enumerator = static_cast<IMMDeviceEnumerator*>(m_enumerator);
    if (!enumerator) {
        const HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                            __uuidof(IMMDeviceEnumerator),
                                            reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr) || !enumerator) {
            GAZER_WARN << "SystemVolume: no device enumerator";
            return false;
        }
        m_enumerator = enumerator;
    }
    IMMDevice* device = nullptr;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)) || !device) {
        return false;
    }
    IAudioEndpointVolume* endpoint = nullptr;
    const HRESULT ok =
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&endpoint));
    device->Release();
    if (FAILED(ok) || !endpoint) {
        return false;
    }
    auto* notify = new SystemVolumeNotify(this);
    if (FAILED(endpoint->RegisterControlChangeNotify(notify))) {
        notify->detach();
        notify->Release();
        notify = nullptr;
    }
    m_endpoint = endpoint;
    m_notify = notify;
    return true;
#else
    return false;
#endif
}

void SystemVolume::releaseEndpoint()
{
#ifdef Q_OS_WIN
    auto* endpoint = static_cast<IAudioEndpointVolume*>(m_endpoint);
    auto* notify = static_cast<SystemVolumeNotify*>(m_notify);
    if (endpoint && notify) {
        endpoint->UnregisterControlChangeNotify(notify);
    }
    if (notify) {
        notify->detach();
        notify->Release();
    }
    if (endpoint) {
        endpoint->Release();
    }
    auto* enumerator = static_cast<IMMDeviceEnumerator*>(m_enumerator);
    if (enumerator) {
        enumerator->Release();
    }
#endif
    m_endpoint = nullptr;
    m_notify = nullptr;
    m_enumerator = nullptr;
}

int SystemVolume::percent() const
{
#ifdef Q_OS_WIN
    auto* endpoint = static_cast<IAudioEndpointVolume*>(m_endpoint);
    if (!endpoint) {
        return 0;
    }
    BOOL mute = FALSE;
    if (SUCCEEDED(endpoint->GetMute(&mute)) && mute) {
        return 0;
    }
    float scalar = 0.0f;
    if (FAILED(endpoint->GetMasterVolumeLevelScalar(&scalar))) {
        return 0;
    }
    return clampPercent(int(qRound(double(scalar) * 100.0)));
#else
    return 0;
#endif
}

bool SystemVolume::muted() const
{
#ifdef Q_OS_WIN
    auto* endpoint = static_cast<IAudioEndpointVolume*>(m_endpoint);
    if (!endpoint) {
        return false;
    }
    BOOL mute = FALSE;
    return SUCCEEDED(endpoint->GetMute(&mute)) && mute;
#else
    return false;
#endif
}

bool SystemVolume::setPercent(int percent)
{
#ifdef Q_OS_WIN
    if (!ensureEndpoint()) {
        return false;
    }
    auto* endpoint = static_cast<IAudioEndpointVolume*>(m_endpoint);
    const int p = clampPercent(percent);
    m_setting = true;
    endpoint->SetMute(FALSE, nullptr);
    const HRESULT hr = endpoint->SetMasterVolumeLevelScalar(float(p) / 100.0f, nullptr);
    m_setting = false;
    if (FAILED(hr)) {
        return false;
    }
    emit changed();
    return true;
#else
    Q_UNUSED(percent);
    return false;
#endif
}

bool SystemVolume::setMuted(bool on)
{
#ifdef Q_OS_WIN
    if (!ensureEndpoint()) {
        return false;
    }
    auto* endpoint = static_cast<IAudioEndpointVolume*>(m_endpoint);
    m_setting = true;
    const HRESULT hr = endpoint->SetMute(on ? TRUE : FALSE, nullptr);
    m_setting = false;
    if (FAILED(hr)) {
        return false;
    }
    emit changed();
    return true;
#else
    Q_UNUSED(on);
    return false;
#endif
}

bool SystemVolume::nudge(int dir)
{
    if (dir == 0) {
        return false;
    }
    if (muted()) {
        if (dir < 0) {
            return true;
        }
        return setMuted(false);
    }
    return setPercent(percent() + (dir > 0 ? kStep : -kStep));
}

void SystemVolume::onEndpointChanged()
{
    if (m_setting) {
        return;
    }
    emit changed();
}

} // namespace gazer
