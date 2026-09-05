#include "assist/TtsService.h"

#include "utils/Log.h"

#include <QTimer>
#include <string>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <sapi.h>
#endif

namespace gazer {

#ifdef Q_OS_WIN
namespace {

class TtsNotifySink final : public ISpNotifySink {
public:
    explicit TtsNotifySink(TtsService* tts)
        : m_tts(tts)
    {
    }

    void detach() { m_tts = nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_ISpNotifySink) {
            *ppv = static_cast<ISpNotifySink*>(this);
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

    HRESULT STDMETHODCALLTYPE Notify() override
    {
        if (m_tts) {
            QMetaObject::invokeMethod(m_tts, "onSapiNotify", Qt::QueuedConnection);
        }
        return S_OK;
    }

private:
    TtsService* m_tts = nullptr;
    LONG m_ref = 1;
};

void freeEvent(SPEVENT* ev)
{
    if (!ev) {
        return;
    }
    if (ev->elParamType == SPET_LPARAM_IS_POINTER || ev->elParamType == SPET_LPARAM_IS_STRING) {
        CoTaskMemFree(reinterpret_cast<void*>(ev->lParam));
        ev->lParam = 0;
    } else if (ev->elParamType == SPET_LPARAM_IS_TOKEN || ev->elParamType == SPET_LPARAM_IS_OBJECT) {
        if (ev->lParam) {
            reinterpret_cast<IUnknown*>(ev->lParam)->Release();
            ev->lParam = 0;
        }
    }
}

} // namespace
#endif

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
    attachNotify();
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
        auto* voice = static_cast<ISpVoice*>(m_voice);
        voice->SetNotifySink(nullptr);
        voice->Release();
        m_voice = nullptr;
    }
    if (m_notifySink) {
        auto* sink = static_cast<TtsNotifySink*>(m_notifySink);
        sink->detach();
        sink->Release();
        m_notifySink = nullptr;
    }
#endif
}

void TtsService::attachNotify()
{
#ifdef Q_OS_WIN
    auto* voice = static_cast<ISpVoice*>(m_voice);
    if (!voice) {
        return;
    }
    auto* sink = new TtsNotifySink(this);
    m_notifySink = sink;
    const ULONGLONG mask = SPFEI(SPEI_END_INPUT_STREAM);
    voice->SetInterest(mask, mask);
    const HRESULT hr = voice->SetNotifySink(sink);
    m_poll = new QTimer(this);
    m_poll->setInterval(50);
    connect(m_poll, &QTimer::timeout, this, &TtsService::pollSapiStatus);
    if (FAILED(hr)) {
        GAZER_WARN << "TtsService: SetNotifySink failed, polling SAPI status";
    }
#endif
}

void TtsService::emitFinishedIfSpeaking()
{
    if (!m_speaking) {
        return;
    }
    m_speaking = false;
    m_seenSpeaking = false;
    m_pollTicks = 0;
    if (m_poll) {
        m_poll->stop();
    }
    emit finished();
}

void TtsService::drainEndEvents()
{
#ifdef Q_OS_WIN
    auto* voice = static_cast<ISpVoice*>(m_voice);
    if (!voice || !m_speaking) {
        return;
    }
    SPEVENT ev{};
    ULONG fetched = 0;
    while (voice->GetEvents(1, &ev, &fetched) == S_OK && fetched > 0) {
        const bool end = ev.eEventId == SPEI_END_INPUT_STREAM && ev.ulStreamNum == m_stream;
        freeEvent(&ev);
        fetched = 0;
        if (end) {
            emitFinishedIfSpeaking();
            return;
        }
    }
#endif
}

void TtsService::onSapiNotify()
{
    drainEndEvents();
}

void TtsService::pollSapiStatus()
{
#ifdef Q_OS_WIN
    auto* voice = static_cast<ISpVoice*>(m_voice);
    if (!voice || !m_speaking) {
        if (m_poll) {
            m_poll->stop();
        }
        return;
    }
    SPVOICESTATUS st{};
    if (FAILED(voice->GetStatus(&st, nullptr))) {
        return;
    }
    ++m_pollTicks;
    if (st.dwRunningState == SPRS_IS_SPEAKING) {
        m_seenSpeaking = true;
        return;
    }
    if (st.dwRunningState != SPRS_DONE) {
        return;
    }
    // Short utterances can skip IS_SPEAKING between 50 ms ticks. After two
    // DONE samples (or one after we saw speaking), treat the stream as ended.
    if (m_seenSpeaking || m_pollTicks >= 2) {
        emitFinishedIfSpeaking();
    }
#endif
}

void TtsService::setSpeed(double speed)
{
#ifdef Q_OS_WIN
    auto* voice = static_cast<ISpVoice*>(m_voice);
    if (!voice) {
        return;
    }
    speed = qBound(0.5, speed, 2.0);
    const long rate = speed >= 1.0 ? long(qRound((speed - 1.0) * 10.0))
                                   : long(qRound((speed - 1.0) * 20.0));
    voice->SetRate(qBound(-10L, rate, 10L));
#else
    Q_UNUSED(speed);
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
    if (m_speaking) {
        voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
        emitFinishedIfSpeaking();
    }
    ULONG stream = 0;
    const std::wstring w = text.toStdWString();
    const HRESULT hr = voice->Speak(w.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, &stream);
    if (FAILED(hr)) {
        if (error) {
            *error = QStringLiteral("Speak failed");
        }
        emit failed(QStringLiteral("Speak failed"));
        return false;
    }
    m_stream = stream;
    m_speaking = true;
    m_seenSpeaking = false;
    m_pollTicks = 0;
    if (m_poll) {
        m_poll->start();
    }
    emit started(text);
    return true;
#else
    if (m_speaking) {
        emitFinishedIfSpeaking();
    }
    GAZER_INFO << "[tts stub]" << text;
    m_speaking = true;
    emit started(text);
    QTimer::singleShot(0, this, [this]() { emitFinishedIfSpeaking(); });
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
    emitFinishedIfSpeaking();
}

#ifdef Q_OS_WIN
namespace {

QString langFromLcidHex(const QString& hex)
{
    bool ok = false;
    const uint lcid = hex.trimmed().toUInt(&ok, 16);
    if (!ok) {
        return {};
    }
    switch (PRIMARYLANGID(LANGID(lcid))) {
    case LANG_ENGLISH:
        return QStringLiteral("en");
    case LANG_SPANISH:
        return QStringLiteral("es");
    case LANG_FRENCH:
        return QStringLiteral("fr");
    case LANG_GERMAN:
        return QStringLiteral("de");
    case LANG_ITALIAN:
        return QStringLiteral("it");
    case LANG_PORTUGUESE:
        return QStringLiteral("pt");
    case LANG_JAPANESE:
        return QStringLiteral("ja");
    case LANG_CHINESE:
        return QStringLiteral("zh");
    default:
        return {};
    }
}

QString sapString(ISpDataKey* key, const WCHAR* name)
{
    if (!key) {
        return {};
    }
    WCHAR* raw = nullptr;
    if (FAILED(key->GetStringValue(name, &raw)) || !raw) {
        return {};
    }
    const QString s = QString::fromWCharArray(raw);
    CoTaskMemFree(raw);
    return s;
}

} // namespace
#endif

QVector<TtsService::VoiceInfo> TtsService::listVoices() const
{
    QVector<VoiceInfo> out;
#ifdef Q_OS_WIN
    ISpObjectTokenCategory* cat = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                                  IID_ISpObjectTokenCategory, reinterpret_cast<void**>(&cat));
    if (FAILED(hr) || !cat) {
        return out;
    }
    hr = cat->SetId(SPCAT_VOICES, FALSE);
    IEnumSpObjectTokens* en = nullptr;
    if (SUCCEEDED(hr)) {
        hr = cat->EnumTokens(nullptr, nullptr, &en);
    }
    if (FAILED(hr) || !en) {
        cat->Release();
        return out;
    }
    ISpObjectToken* token = nullptr;
    while (en->Next(1, &token, nullptr) == S_OK && token) {
        VoiceInfo info;
        WCHAR* id = nullptr;
        if (SUCCEEDED(token->GetId(&id)) && id) {
            info.token = QString::fromWCharArray(id);
            CoTaskMemFree(id);
        }
        WCHAR* name = nullptr;
        if (SUCCEEDED(token->GetStringValue(nullptr, &name)) && name) {
            info.name = QString::fromWCharArray(name);
            CoTaskMemFree(name);
        }
        ISpDataKey* attrs = nullptr;
        if (SUCCEEDED(token->OpenKey(L"Attributes", &attrs)) && attrs) {
            info.gender = sapString(attrs, L"Gender").trimmed().toLower();
            if (info.gender.startsWith(QLatin1Char('f'))) {
                info.gender = QStringLiteral("female");
            } else if (info.gender.startsWith(QLatin1Char('m'))) {
                info.gender = QStringLiteral("male");
            } else {
                info.gender.clear();
            }
            info.language = langFromLcidHex(sapString(attrs, L"Language"));
            attrs->Release();
        }
        if (!info.token.isEmpty() && !info.name.isEmpty()) {
            out.push_back(info);
        }
        token->Release();
        token = nullptr;
    }
    en->Release();
    cat->Release();
#endif
    return out;
}

bool TtsService::setVoiceToken(const QString& token, QString* error)
{
#ifdef Q_OS_WIN
    auto* voice = static_cast<ISpVoice*>(m_voice);
    if (!voice) {
        if (error) {
            *error = QStringLiteral("TTS not available");
        }
        return false;
    }
    const QString id = token.trimmed();
    if (id.isEmpty()) {
        voice->SetVoice(nullptr);
        m_voiceToken.clear();
        return true;
    }
    ISpObjectToken* tok = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectToken, nullptr, CLSCTX_ALL, IID_ISpObjectToken,
                                  reinterpret_cast<void**>(&tok));
    if (SUCCEEDED(hr) && tok) {
        const std::wstring w = id.toStdWString();
        hr = tok->SetId(nullptr, w.c_str(), FALSE);
    }
    if (FAILED(hr) || !tok) {
        if (tok) {
            tok->Release();
        }
        if (error) {
            *error = QStringLiteral("Unknown SAPI voice");
        }
        return false;
    }
    hr = voice->SetVoice(tok);
    tok->Release();
    if (FAILED(hr)) {
        if (error) {
            *error = QStringLiteral("Could not select SAPI voice");
        }
        return false;
    }
    m_voiceToken = id;
    return true;
#else
    m_voiceToken = token.trimmed();
    Q_UNUSED(error);
    return true;
#endif
}

} // namespace gazer
