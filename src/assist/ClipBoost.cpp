#include "assist/ClipBoost.h"

#include "assist/AudioGain.h"
#include "utils/Log.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QDataStream>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace {

constexpr int kMaxPcmBytes = 50 * 1024 * 1024;

QByteArray makeWav(const QByteArray& pcm, int sampleRate, int channels)
{
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);
    const int dataSize = pcm.size();
    const int ch = qMax(1, channels);
    const int rate = qMax(1, sampleRate);
    const quint16 blockAlign = quint16(ch * 2);
    ds.writeRawData("RIFF", 4);
    ds << quint32(36 + dataSize);
    ds.writeRawData("WAVE", 4);
    ds.writeRawData("fmt ", 4);
    ds << quint32(16);
    ds << quint16(1);
    ds << quint16(ch);
    ds << quint32(rate);
    ds << quint32(rate * blockAlign);
    ds << blockAlign;
    ds << quint16(16);
    ds.writeRawData("data", 4);
    ds << quint32(dataSize);
    ds.writeRawData(pcm.constData(), dataSize);
    return out;
}

QByteArray pcmInt16(const QAudioBuffer& buf, double gain)
{
    const QAudioFormat fmt = buf.format();
    if (fmt.sampleFormat() == QAudioFormat::Int16) {
        const auto* src = buf.constData<qint16>();
        QByteArray chunk(reinterpret_cast<const char*>(src), int(buf.byteCount()));
        AudioGain::scaleInt16(chunk.data(), chunk.size(), gain);
        return chunk;
    }
    if (fmt.sampleFormat() == QAudioFormat::Float) {
        const auto* src = buf.constData<float>();
        const qsizetype n = buf.sampleCount();
        QByteArray chunk;
        chunk.resize(int(n * 2));
        auto* dst = reinterpret_cast<qint16*>(chunk.data());
        for (qsizetype i = 0; i < n; ++i) {
            const int v = int(std::lround(double(src[i]) * gain * 32767.0));
            dst[i] = qint16(std::clamp(v, -32768, 32767));
        }
        return chunk;
    }
    if (fmt.sampleFormat() == QAudioFormat::Int32) {
        const auto* src = buf.constData<qint32>();
        const qsizetype n = buf.sampleCount();
        QByteArray chunk;
        chunk.resize(int(n * 2));
        auto* dst = reinterpret_cast<qint16*>(chunk.data());
        for (qsizetype i = 0; i < n; ++i) {
            const double x = (double(src[i]) / 2147483648.0) * gain;
            const int v = int(std::lround(x * 32767.0));
            dst[i] = qint16(std::clamp(v, -32768, 32767));
        }
        return chunk;
    }
    return {};
}

} // namespace

ClipBoost::ClipBoost(QObject* parent)
    : QObject(parent)
    , m_decoder(new QAudioDecoder(this))
{
    connect(m_decoder, &QAudioDecoder::bufferReady, this, &ClipBoost::onBuffer);
    connect(m_decoder, &QAudioDecoder::finished, this, &ClipBoost::onFinished);
    connect(m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) { onError(); });
}

bool ClipBoost::isSupported() const
{
    return m_decoder && m_decoder->isSupported();
}

void ClipBoost::cancel()
{
    ++m_gen;
    m_running = false;
    m_pcm.clear();
    m_sampleRate = 0;
    m_channels = 0;
    if (m_decoder->isDecoding()) {
        m_decoder->stop();
    }
    m_decoder->setSource(QUrl());
}

void ClipBoost::start(const QString& path, double gain)
{
    cancel();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !isSupported()) {
        emit failed();
        return;
    }
    m_gain = AudioGain::clamp(gain);
    m_running = true;
    const quint64 gen = m_gen;
    m_decoder->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
    m_decoder->start();
    if (m_decoder->error() != QAudioDecoder::NoError) {
        GAZER_WARN << "ClipBoost:" << m_decoder->errorString();
        if (gen == m_gen) {
            fail();
        }
        return;
    }
}

void ClipBoost::fail()
{
    if (!m_running) {
        return;
    }
    cancel();
    emit failed();
}

void ClipBoost::onBuffer()
{
    if (!m_running) {
        return;
    }
    while (m_decoder->bufferAvailable()) {
        const QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid() || buf.byteCount() <= 0) {
            continue;
        }
        const QAudioFormat fmt = buf.format();
        if (m_sampleRate == 0) {
            m_sampleRate = fmt.sampleRate();
            m_channels = fmt.channelCount();
        } else if (fmt.sampleRate() != m_sampleRate || fmt.channelCount() != m_channels) {
            continue;
        }
        QByteArray chunk = pcmInt16(buf, m_gain);
        if (chunk.isEmpty()) {
            fail();
            return;
        }
        if (m_pcm.size() + chunk.size() > kMaxPcmBytes) {
            GAZER_WARN << "ClipBoost: PCM exceeds cap";
            fail();
            return;
        }
        m_pcm.append(chunk);
    }
}

void ClipBoost::onFinished()
{
    if (!m_running) {
        return;
    }
    onBuffer();
    if (!m_running) {
        return;
    }
    if (m_pcm.isEmpty() || m_sampleRate <= 0 || m_channels <= 0) {
        fail();
        return;
    }
    m_running = false;
    const QByteArray wav = makeWav(m_pcm, m_sampleRate, m_channels);
    m_pcm.clear();
    emit finished(wav);
}

void ClipBoost::onError()
{
    if (!m_running) {
        return;
    }
    GAZER_WARN << "ClipBoost:" << m_decoder->errorString();
    fail();
}

} // namespace gazer
