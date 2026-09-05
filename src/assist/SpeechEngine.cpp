#include "assist/SpeechEngine.h"

#include "app/AppSettings.h"
#include "assist/ClipPlayer.h"
#include "assist/ElevenClient.h"
#include "assist/SpeechSecrets.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTimer>

namespace gazer {
namespace {

constexpr int kLatchAfterFails = 3;
constexpr qint64 kMaxClipBytes = 5 * 1024 * 1024;

} // namespace

SpeechEngine::SpeechEngine(TtsService& tts, AppSettings& settings, SpeechSecrets& secrets,
                           ElevenClient& eleven, ClipPlayer& clips, QObject* parent)
    : QObject(parent)
    , m_tts(tts)
    , m_settings(settings)
    , m_secrets(secrets)
    , m_eleven(eleven)
    , m_clips(clips)
{
    connect(&m_tts, &TtsService::started, this, [this](const QString&) { onTtsStarted(); });
    connect(&m_tts, &TtsService::finished, this, &SpeechEngine::onTtsFinished);
    connect(&m_tts, &TtsService::failed, this, &SpeechEngine::onTtsFailed);
    connect(&m_eleven, &ElevenClient::speechReady, this, &SpeechEngine::onSpeechReady);
    connect(&m_eleven, &ElevenClient::speechFailed, this, &SpeechEngine::onSpeechFailed);
    connect(&m_clips, &ClipPlayer::started, this, &SpeechEngine::onClipStarted);
    connect(&m_clips, &ClipPlayer::stopped, this, &SpeechEngine::onClipStopped);
    connect(&m_clips, &ClipPlayer::failed, this, &SpeechEngine::onClipFailed);
}

SpeechEngine::~SpeechEngine()
{
    m_eleven.abortSpeech();
    m_clips.stop();
    m_tts.stop();
    removeIfTemp(m_tmpMpeg);
    removeIfTemp(m_lastClip.path);
}

QString SpeechEngine::tmpMpegPath() const
{
    const QString dir = QDir(ElevenClient::speechDir()).filePath(QStringLiteral("tmp"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("%1.mp3").arg(m_generation));
}

void SpeechEngine::removeIfTemp(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fi(path);
    if (fi.dir().dirName() == QLatin1String("tmp")) {
        QFile::remove(path);
    }
}

void SpeechEngine::discardTmpIfUnretained()
{
    if (m_tmpMpeg.isEmpty() || m_tmpMpeg == m_lastClip.path) {
        return;
    }
    removeIfTemp(m_tmpMpeg);
    m_tmpMpeg.clear();
}

void SpeechEngine::setLastClip(const QString& path, const QString& phrase)
{
    const QString prev = m_lastClip.path;
    m_lastClip.path = path;
    m_lastClip.phrase = phrase;
    if (!prev.isEmpty() && prev != path) {
        removeIfTemp(prev);
    }
}

void SpeechEngine::keepGeneratedClip(const QString& stablePath)
{
    if (stablePath.isEmpty() || !QFileInfo::exists(stablePath)) {
        return;
    }
    setLastClip(stablePath, m_lastClip.phrase);
    discardTmpIfUnretained();
}

void SpeechEngine::cancelInFlight()
{
    ++m_generation;
    m_retried429 = false;
    m_eleven.abortSpeech();
    m_clips.stop();
    m_tts.stop();
    discardTmpIfUnretained();
}

QString SpeechEngine::elevenConfigFingerprint() const
{
    QString key;
    (void)m_secrets.load(&key);
    return ElevenRequest::normalizeModelId(m_settings.speechModel) + QLatin1Char('\n')
           + m_settings.elevenVoiceId.trimmed() + QLatin1Char('\n')
           + QString::number(qHash(key.trimmed()));
}

void SpeechEngine::maybeResetElevenLatch()
{
    const QString fp = elevenConfigFingerprint();
    if (fp == m_elevenConfigFp) {
        return;
    }
    m_elevenConfigFp = fp;
    m_elevenLatched = false;
    m_elevenFails = 0;
}

void SpeechEngine::speak(const QString& phrase, SpeakKind kind, bool recordHistory)
{
    cancelInFlight();
    maybeResetElevenLatch();
    m_kind = kind;
    m_recordHistory = recordHistory && kind == SpeakKind::Composed;
    m_historyPhrase = phrase;
    m_pendingSpoken.clear();
    m_pendingVoiceId.clear();

    if (!ElevenRequest::hasNonTagSpeechContent(phrase)) {
        m_recordHistory = false;
        if (kind == SpeakKind::Composed && ElevenRequest::phraseHasInlineTags(phrase)) {
            emit notify(QStringLiteral("Nothing to speak"));
        }
        setIdle();
        emit finished();
        return;
    }

    const QString spoken = ElevenRequest::stripInlineTags(phrase);

    if (kind == SpeakKind::Canned) {
        speakSapi(spoken);
        return;
    }

    const QString model = ElevenRequest::normalizeModelId(m_settings.speechModel);
    const bool wantEleven = ElevenRequest::isElevenModel(model);
    QString key;
    const bool haveKey = m_secrets.load(&key) && !key.trimmed().isEmpty();
    const QString voiceId = m_settings.elevenVoiceId.trimmed();

    if (wantEleven && (!haveKey || voiceId.isEmpty())) {
        emit notify(haveKey ? QStringLiteral("No ElevenLabs voice — using SAPI")
                            : QStringLiteral("No ElevenLabs API key — using SAPI"));
        speakSapi(spoken);
        return;
    }

    if (wantEleven && !m_elevenLatched) {
        startEleven(phrase, voiceId);
        return;
    }

    speakSapi(spoken);
}

void SpeechEngine::previewCurrent()
{
    speak(QStringLiteral("This is a preview."), SpeakKind::Composed, false);
}

void SpeechEngine::previewVoice(const QString& voiceId)
{
    const QString id = voiceId.trimmed();
    if (id.isEmpty()) {
        previewCurrent();
        return;
    }
    const QString model = ElevenRequest::normalizeModelId(m_settings.speechModel);
    if (ElevenRequest::isElevenModel(model)) {
        const QString prev = m_settings.elevenVoiceId;
        m_settings.elevenVoiceId = id;
        previewCurrent();
        m_settings.elevenVoiceId = prev;
        return;
    }
    const QString prev = m_settings.sapiVoiceToken;
    m_settings.sapiVoiceToken = id;
    (void)m_tts.setVoiceToken(id);
    previewCurrent();
    m_settings.sapiVoiceToken = prev;
    (void)m_tts.setVoiceToken(prev);
}

void SpeechEngine::speakSapi(const QString& spoken)
{
    m_lastBackend = Backend::Sapi;
    m_sapiGen = m_generation;
    if (m_kind == SpeakKind::Composed && m_recordHistory) {
        setLastClip({}, spoken);
    }
    if (spoken.isEmpty()) {
        setIdle();
        emit finished();
        return;
    }
    if (!m_tts.isAvailable()) {
        m_status.lastError.clear();
        setIdle();
        emit finished();
        return;
    }

    m_status.busy = true;
    m_status.speaking = true;
    m_status.lastError.clear();
    emit statusChanged();

    m_tts.setSpeed(m_settings.speechSpeed);
    QString err;
    if (!m_tts.speak(spoken, &err)) {
        return;
    }
}

void SpeechEngine::startEleven(const QString& phrase, const QString& voiceId)
{
    QString key;
    if (!m_secrets.load(&key) || key.trimmed().isEmpty()) {
        emit notify(QStringLiteral("Set an ElevenLabs API key and voice first"));
        setIdle();
        emit finished();
        return;
    }

    m_lastBackend = Backend::Eleven;
    m_elevenGen = m_generation;
    m_pendingPrep = ElevenRequest::prepareSpeakRequest(phrase, m_settings.speechModel,
                                                       m_settings.speechSpeed, m_settings.speechPitch);
    m_pendingSpoken = ElevenRequest::stripInlineTags(phrase);
    m_pendingVoiceId = voiceId;
    m_retried429 = false;

    if (!ElevenRequest::isElevenModel(m_pendingPrep.modelId)) {
        speakSapi(m_pendingSpoken);
        return;
    }

    m_status.busy = true;
    m_status.speaking = false;
    m_status.lastError.clear();
    emit statusChanged();

    GAZER_INFO << "ElevenLabs speak model" << m_pendingPrep.modelId << "voice" << voiceId;
    m_eleven.fetchSpeech(key, voiceId, m_pendingPrep.body);
}

void SpeechEngine::fallbackSapi(const QString& spoken)
{
    speakSapi(spoken.isEmpty() ? m_pendingSpoken : spoken);
}

void SpeechEngine::latchIfNeeded()
{
    ++m_elevenFails;
    if (!m_elevenLatched && m_elevenFails >= kLatchAfterFails) {
        m_elevenLatched = true;
        emit notify(QStringLiteral("ElevenLabs failed three times — using SAPI this session"));
    }
}

bool SpeechEngine::writeMpegTemp(const QByteArray& mpeg, QString* pathOut)
{
    if (mpeg.isEmpty() || !pathOut) {
        return false;
    }
    if (mpeg.size() > kMaxClipBytes) {
        GAZER_WARN << "SpeechEngine: clip" << mpeg.size() << "bytes exceeds 5 MB cap";
        return false;
    }
    const QString path = tmpMpegPath();
    if (m_tmpMpeg != path) {
        discardTmpIfUnretained();
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        GAZER_WARN << "SpeechEngine: cannot write" << path;
        return false;
    }
    if (f.write(mpeg) != mpeg.size()) {
        f.remove();
        return false;
    }
    f.close();
    m_tmpMpeg = path;
    *pathOut = path;
    return true;
}

void SpeechEngine::playMpeg(const QString& path, double localSpeed, const QString& spokenFallback)
{
    m_clipGen = m_generation;
    if (m_clips.isAvailable() && m_clips.play(path, localSpeed, m_settings.speechVolume)) {
        m_status.busy = true;
        m_status.speaking = true;
        emit statusChanged();
        return;
    }
    GAZER_WARN << "ClipPlayer unavailable; SAPI fallback";
    fallbackSapi(spokenFallback);
}

void SpeechEngine::stop()
{
    cancelInFlight();
    setIdle();
    emit finished();
}

void SpeechEngine::emitHistoryIfNeeded(const QString& backend, const QString& modelId,
                                       const QString& voiceId)
{
    if (!m_recordHistory || m_historyPhrase.trimmed().isEmpty()) {
        return;
    }
    m_recordHistory = false;
    const QString mpeg = (backend == QLatin1String("eleven") && !m_lastClip.path.isEmpty()
                          && QFileInfo::exists(m_lastClip.path))
                             ? m_lastClip.path
                             : QString();
    emit historyReady(m_historyPhrase, backend, modelId, voiceId, mpeg);
}

bool SpeechEngine::playFile(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return false;
    }
    cancelInFlight();
    m_kind = SpeakKind::Composed;
    m_recordHistory = false;
    m_historyPhrase.clear();
    m_pendingSpoken.clear();
    m_lastBackend = Backend::Eleven;
    m_status.busy = true;
    m_status.speaking = false;
    m_status.lastError.clear();
    emit statusChanged();
    m_clipGen = m_generation;
    if (m_clips.isAvailable() && m_clips.play(path, 1.0, m_settings.speechVolume)) {
        m_status.speaking = true;
        emit statusChanged();
        return true;
    }
    setIdle();
    emit failed(QStringLiteral("Clip play failed"));
    return false;
}

void SpeechEngine::setIdle()
{
    const bool changed = m_status.busy || m_status.speaking;
    m_status.busy = false;
    m_status.speaking = false;
    if (changed) {
        emit statusChanged();
    }
}

void SpeechEngine::onTtsStarted()
{
    if (m_sapiGen != m_generation || m_lastBackend != Backend::Sapi) {
        return;
    }
    if (!m_status.busy || !m_status.speaking) {
        m_status.busy = true;
        m_status.speaking = true;
        emit statusChanged();
    }
}

void SpeechEngine::onTtsFinished()
{
    if (m_sapiGen != m_generation || m_lastBackend != Backend::Sapi) {
        return;
    }
    setIdle();
    emitHistoryIfNeeded(QStringLiteral("sapi"), {}, m_settings.sapiVoiceToken.trimmed());
    emit finished();
}

void SpeechEngine::onTtsFailed(const QString& error)
{
    if (m_sapiGen != m_generation || m_lastBackend != Backend::Sapi) {
        return;
    }
    m_status.lastError = error;
    setIdle();
    emit failed(error);
}

void SpeechEngine::onSpeechReady(const QByteArray& mpeg)
{
    if (m_elevenGen != m_generation || !m_status.busy || m_lastBackend != Backend::Eleven) {
        return;
    }
    m_elevenFails = 0;
    QString path;
    if (!writeMpegTemp(mpeg, &path)) {
        latchIfNeeded();
        fallbackSapi(m_pendingSpoken);
        return;
    }
    if (m_recordHistory) {
        setLastClip(path, m_pendingSpoken);
        emitHistoryIfNeeded(QStringLiteral("eleven"), m_pendingPrep.modelId, m_pendingVoiceId);
    }
    const QString playPath = (m_recordHistory && !m_lastClip.path.isEmpty()) ? m_lastClip.path : path;
    playMpeg(playPath, m_pendingPrep.localSpeed, m_pendingSpoken);
}

void SpeechEngine::onSpeechFailed(int httpStatus, const QString& error, int retryAfterMs)
{
    if (m_elevenGen != m_generation || !m_status.busy || m_lastBackend != Backend::Eleven) {
        return;
    }
    if (httpStatus == 401) {
        QString err;
        (void)m_secrets.clear(&err);
        m_settings.elevenApiKeySet = m_secrets.hasKey();
        emit notify(error.isEmpty() ? QStringLiteral("Invalid API key") : error);
        fallbackSapi(m_pendingSpoken);
        return;
    }
    if (httpStatus == 403) {
        emit notify(error.isEmpty() ? QStringLiteral("ElevenLabs forbidden") : error);
        latchIfNeeded();
        fallbackSapi(m_pendingSpoken);
        return;
    }
    if (httpStatus == 429 && !m_retried429) {
        m_retried429 = true;
        emit notify(QStringLiteral("ElevenLabs busy — try again"));
        const int waitMs = retryAfterMs > 0 ? retryAfterMs : 1000;
        const quint64 gen = m_generation;
        QTimer::singleShot(waitMs, this, [this, gen]() {
            if (gen != m_generation || m_lastBackend != Backend::Eleven || !m_status.busy) {
                return;
            }
            QString key;
            if (!m_secrets.load(&key) || key.isEmpty()) {
                fallbackSapi(m_pendingSpoken);
                return;
            }
            m_eleven.fetchSpeech(key, m_pendingVoiceId, m_pendingPrep.body);
        });
        return;
    }
    if (!error.isEmpty()) {
        emit notify(error);
    }
    latchIfNeeded();
    fallbackSapi(m_pendingSpoken);
}

void SpeechEngine::onClipStarted()
{
    if (m_clipGen != m_generation || !m_status.busy || m_lastBackend != Backend::Eleven) {
        return;
    }
    if (!m_status.speaking) {
        m_status.speaking = true;
        emit statusChanged();
    }
}

void SpeechEngine::onClipStopped()
{
    if (m_clipGen != m_generation || !m_status.busy || m_lastBackend != Backend::Eleven) {
        return;
    }
    discardTmpIfUnretained();
    setIdle();
    emit finished();
}

void SpeechEngine::onClipFailed(const QString& error)
{
    if (m_clipGen != m_generation || !m_status.busy || m_lastBackend != Backend::Eleven) {
        return;
    }
    GAZER_WARN << "ClipPlayer:" << error;
    discardTmpIfUnretained();
    if (!m_pendingSpoken.isEmpty()) {
        fallbackSapi(m_pendingSpoken);
        return;
    }
    setIdle();
    emit failed(error);
}

} // namespace gazer
