#include "assist/TtsService.h"

#include "utils/Log.h"

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <sapi.h>
#  include <comdef.h>
#endif

namespace gazer {

TtsService::TtsService(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // S_FALSE = already initialized on this thread — fine
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        GAZER_WARN << "TtsService: CoInitializeEx failed" << Qt::hex << hr;
        return;
    }

    ISpVoice* voice = nullptr;
    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice,
                          reinterpret_cast<void**>(&voice));
    if (FAILED(hr) || !voice) {
        GAZER_WARN << "TtsService: SpVoice unavailable";
        return;
    }
    m_voice = voice;
    m_available = true;
    GAZER_INFO << "TtsService: Windows SAPI ready";
#else
    GAZER_INFO << "TtsService: stub (non-Windows)";
#endif
}

TtsService::~TtsService()
{
    stop();
#ifdef Q_OS_WIN
    if (m_voice) {
        static_cast<ISpVoice*>(m_voice)->Release();
        m_voice = nullptr;
    }
#endif
}

bool TtsService::speak(const QString& text, QString* error)
{
    if (text.isEmpty()) {
        return true;
    }
#ifdef Q_OS_WIN
    if (!m_voice) {
        if (error) {
            *error = QStringLiteral("TTS not available");
        }
        emit failed(QStringLiteral("TTS not available"));
        return false;
    }
    auto* voice = static_cast<ISpVoice*>(m_voice);
    // Stop previous utterance so rapid dwell speaks the latest phrase.
    voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    const std::wstring w = text.toStdWString();
    const HRESULT hr = voice->Speak(w.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    if (FAILED(hr)) {
        if (error) {
            *error = QStringLiteral("Speak failed");
        }
        emit failed(QStringLiteral("Speak failed"));
        return false;
    }
    emit started(text);
    return true;
#else
    GAZER_INFO << "[tts stub]" << text;
    emit started(text);
    Q_UNUSED(error);
    return true;
#endif
}

void TtsService::stop()
{
#ifdef Q_OS_WIN
    if (m_voice) {
        static_cast<ISpVoice*>(m_voice)->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
#endif
}

} // namespace gazer
