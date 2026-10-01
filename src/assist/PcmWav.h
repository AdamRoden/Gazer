#pragma once

#include <QByteArray>
#include <QFile>
#include <QtGlobal>

namespace gazer::PcmWav {

/// ElevenLabs `pcm_24000`: signed 16-bit little-endian mono, no header.
inline constexpr int kSampleRate = 24000;

[[nodiscard]] inline QByteArray wrapPcm16Mono(const QByteArray& pcm, int sampleRate)
{
    const quint32 rate = quint32(qMax(1, sampleRate));
    const quint32 dataBytes = quint32(pcm.size() - (pcm.size() & 1));
    const quint32 riffSize = 36u + dataBytes;
    QByteArray out;
    out.reserve(44 + int(dataBytes));
    out.append("RIFF", 4);
    const auto le32 = [](quint32 v) {
        QByteArray b(4, '\0');
        b[0] = char(v & 0xFF);
        b[1] = char((v >> 8) & 0xFF);
        b[2] = char((v >> 16) & 0xFF);
        b[3] = char((v >> 24) & 0xFF);
        return b;
    };
    const auto le16 = [](quint16 v) {
        QByteArray b(2, '\0');
        b[0] = char(v & 0xFF);
        b[1] = char((v >> 8) & 0xFF);
        return b;
    };
    out.append(le32(riffSize));
    out.append("WAVE", 4);
    out.append("fmt ", 4);
    out.append(le32(16));
    out.append(le16(1));
    out.append(le16(1));
    out.append(le32(rate));
    out.append(le32(rate * 2));
    out.append(le16(2));
    out.append(le16(16));
    out.append("data", 4);
    out.append(le32(dataBytes));
    out.append(pcm.constData(), int(dataBytes));
    return out;
}

[[nodiscard]] inline bool writeFile(const QString& path, const QByteArray& pcm, int sampleRate)
{
    if (path.isEmpty() || pcm.size() < 2) {
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray wav = wrapPcm16Mono(pcm, sampleRate);
    return f.write(wav) == wav.size();
}

} // namespace gazer::PcmWav
