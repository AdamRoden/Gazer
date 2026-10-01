#include "assist/PcmStreamPlayer.h"

#include "assist/AudioGain.h"
#include "utils/Log.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace gazer {
namespace {

double clampSpeed(double speed)
{
    if (!std::isfinite(speed)) {
        return 1.0;
    }
    return std::clamp(speed, 0.25, 4.0);
}

} // namespace

PcmStreamPlayer::PcmStreamPlayer(QObject* parent)
    : QObject(parent)
    , m_pump(new QTimer(this))
{
    m_pump->setInterval(15);
    connect(m_pump, &QTimer::timeout, this, &PcmStreamPlayer::pump);
}

PcmStreamPlayer::~PcmStreamPlayer()
{
    stopQuiet();
}

bool PcmStreamPlayer::start(int sampleRate, double speed, double gain)
{
    stopQuiet();
    if (sampleRate < 8000) {
        return false;
    }
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull()) {
        GAZER_WARN << "PcmStreamPlayer: no audio output";
        return false;
    }
    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    m_sink = new QAudioSink(dev, fmt, this);
    m_sink->setBufferSize(sampleRate * 2 / 10);
    m_device = m_sink->start();
    if (!m_device) {
        GAZER_WARN << "PcmStreamPlayer: sink did not start";
        m_sink->deleteLater();
        m_sink = nullptr;
        return false;
    }
    m_speed = clampSpeed(speed);
    m_gain = AudioGain::clamp(gain);
    m_bytesPerSec = sampleRate * 2;
    m_bytesAccepted = 0;
    m_pending.clear();
    m_ending = false;
    m_rate.reset();
    m_playing = true;
    m_pump->start();
    emit started();
    return true;
}

void PcmStreamPlayer::push(const QByteArray& pcm)
{
    if (!m_playing || m_ending || pcm.isEmpty()) {
        return;
    }
    QByteArray shaped = m_rate.process(pcm, m_speed);
    if (AudioGain::needsDecode(m_gain)) {
        AudioGain::scaleInt16(shaped.data(), shaped.size(), m_gain);
    }
    if (!shaped.isEmpty()) {
        m_pending.append(shaped);
    }
    pump();
}

void PcmStreamPlayer::finish()
{
    if (!m_playing) {
        emit stopped();
        return;
    }
    QByteArray tail = m_rate.flush(m_speed);
    if (AudioGain::needsDecode(m_gain)) {
        AudioGain::scaleInt16(tail.data(), tail.size(), m_gain);
    }
    if (!tail.isEmpty()) {
        m_pending.append(tail);
    }
    m_ending = true;
    m_finishClock.start();
    m_finishBudgetMs = 250;
    pump();
}

void PcmStreamPlayer::stop()
{
    const bool was = m_playing;
    stopQuiet();
    if (was) {
        emit stopped();
    }
}

void PcmStreamPlayer::pump()
{
    if (!m_playing || !m_device || m_suppress) {
        return;
    }
    while (!m_pending.isEmpty()) {
        const qint64 n = m_device->write(m_pending);
        if (n < 0) {
            fail(QStringLiteral("Audio write failed"));
            return;
        }
        if (n == 0) {
            break;
        }
        m_pending.remove(0, int(n));
        m_bytesAccepted += n;
    }
    if (m_sink) {
        const auto err = m_sink->error();
        if (err == QtAudio::OpenError || err == QtAudio::IOError || err == QtAudio::FatalError) {
            fail(QStringLiteral("Audio output failed"));
            return;
        }
    }
    if (!m_ending || !m_pending.isEmpty() || !m_sink) {
        return;
    }
    const qint64 totalMs = m_bytesPerSec > 0 ? (m_bytesAccepted * 1000) / m_bytesPerSec : 0;
    m_finishBudgetMs = int(totalMs) + 250;
    const qint64 totalUs = m_bytesPerSec > 0 ? (m_bytesAccepted * 1000000) / m_bytesPerSec : 0;
    const qint64 leftUs = totalUs - qint64(m_sink->processedUSecs());
    if (leftUs <= 30000 || m_finishClock.elapsed() >= m_finishBudgetMs) {
        complete();
    }
}

void PcmStreamPlayer::stopQuiet()
{
    m_suppress = true;
    m_pump->stop();
    m_playing = false;
    m_ending = false;
    m_pending.clear();
    m_rate.reset();
    m_device = nullptr;
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    m_suppress = false;
}

void PcmStreamPlayer::complete()
{
    if (!m_playing) {
        return;
    }
    stopQuiet();
    emit stopped();
}

void PcmStreamPlayer::fail(const QString& error)
{
    if (m_suppress) {
        return;
    }
    stopQuiet();
    emit failed(error);
}

} // namespace gazer
