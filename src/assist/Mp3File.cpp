#include "assist/Mp3File.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cstring>

namespace gazer {
namespace {

constexpr int kEncodeRate = 48000;

template <typename T>
struct ComPtr {
    T* p = nullptr;
    ~ComPtr()
    {
        if (p) {
            p->Release();
        }
    }
    ComPtr() = default;
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
};

void fail(QString* error, const QString& msg)
{
    if (error) {
        *error = msg;
    }
}

bool replaceFile(const QString& tmp, const QString& dest, QString* error)
{
    QFile::remove(dest);
    if (QFile::rename(tmp, dest)) {
        return true;
    }
    if (QFile::copy(tmp, dest)) {
        QFile::remove(tmp);
        return true;
    }
    QFile::remove(tmp);
    fail(error, QStringLiteral("Could not save the MP3"));
    return false;
}

bool copyMp3(const QString& source, const QString& dest, QString* error)
{
    const QString tmp = dest + QStringLiteral(".gazer-tmp.mp3");
    QFile::remove(tmp);
    if (!QFile::copy(source, tmp)) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    return replaceFile(tmp, dest, error);
}

QVector<float> toMono(const QByteArray& interleaved, int channels, bool floating)
{
    QVector<float> out;
    if (channels < 1 || interleaved.isEmpty()) {
        return out;
    }
    if (floating) {
        const int frames = interleaved.size() / int(sizeof(float) * channels);
        out.resize(frames);
        const auto* src = reinterpret_cast<const float*>(interleaved.constData());
        for (int i = 0; i < frames; ++i) {
            float sum = 0.f;
            for (int c = 0; c < channels; ++c) {
                sum += src[i * channels + c];
            }
            out[i] = sum / float(channels);
        }
        return out;
    }
    const int frames = interleaved.size() / (2 * channels);
    out.resize(frames);
    const auto* src = reinterpret_cast<const qint16*>(interleaved.constData());
    for (int i = 0; i < frames; ++i) {
        float sum = 0.f;
        for (int c = 0; c < channels; ++c) {
            sum += float(src[i * channels + c]) / 32768.f;
        }
        out[i] = sum / float(channels);
    }
    return out;
}

QVector<float> resample(const QVector<float>& in, int inRate, int outRate)
{
    if (in.isEmpty() || inRate <= 0 || outRate <= 0 || inRate == outRate) {
        return in;
    }
    const double step = double(inRate) / double(outRate);
    const int n = qMax(1, int(double(in.size()) * double(outRate) / double(inRate)));
    QVector<float> out(n);
    const int last = in.size() - 1;
    for (int i = 0; i < n; ++i) {
        const double pos = i * step;
        const int i0 = qMin(int(pos), last);
        const int i1 = qMin(i0 + 1, last);
        const float f = float(pos - i0);
        out[i] = in[i0] * (1.f - f) + in[i1] * f;
    }
    return out;
}

QByteArray toPcm16(const QVector<float>& in)
{
    QByteArray bytes;
    bytes.resize(in.size() * 2);
    auto* dst = reinterpret_cast<qint16*>(bytes.data());
    for (int i = 0; i < in.size(); ++i) {
        const float c = qBound(-1.f, in[i], 1.f);
        dst[i] = qint16(qRound(c * 32767.f));
    }
    return bytes;
}

bool readClip(const QString& source, QVector<float>* mono, int* rate, QString* error)
{
    ComPtr<IMFSourceReader> reader;
    const std::wstring path = QFileInfo(source).absoluteFilePath().toStdWString();
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader.p);
    if (FAILED(hr) || !reader.p) {
        fail(error, QStringLiteral("Could not read the recording"));
        return false;
    }
    ComPtr<IMFMediaType> want;
    hr = MFCreateMediaType(&want.p);
    if (FAILED(hr) || !want.p) {
        fail(error, QStringLiteral("Could not read the recording"));
        return false;
    }
    want.p->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    want.p->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    want.p->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    hr = reader.p->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, want.p);
    if (FAILED(hr)) {
        fail(error, QStringLiteral("Could not read the recording"));
        return false;
    }
    ComPtr<IMFMediaType> got;
    hr = reader.p->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &got.p);
    if (FAILED(hr) || !got.p) {
        fail(error, QStringLiteral("Could not read the recording"));
        return false;
    }
    UINT32 sampleRate = 0;
    UINT32 channels = 0;
    GUID subtype = GUID_NULL;
    got.p->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    got.p->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    got.p->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (sampleRate == 0 || channels == 0) {
        fail(error, QStringLiteral("Could not read the recording"));
        return false;
    }
    const bool floating = IsEqualGUID(subtype, MFAudioFormat_Float);
    QByteArray raw;
    while (true) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader.p->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags,
                                  nullptr, &sample.p);
        if (FAILED(hr)) {
            fail(error, QStringLiteral("Could not read the recording"));
            return false;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
        if (!sample.p) {
            continue;
        }
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample.p->ConvertToContiguousBuffer(&buffer.p)) || !buffer.p) {
            continue;
        }
        BYTE* data = nullptr;
        DWORD len = 0;
        if (FAILED(buffer.p->Lock(&data, nullptr, &len)) || !data || len == 0) {
            continue;
        }
        raw.append(reinterpret_cast<const char*>(data), int(len));
        buffer.p->Unlock();
    }
    *mono = toMono(raw, int(channels), floating);
    *rate = int(sampleRate);
    if (mono->isEmpty()) {
        fail(error, QStringLiteral("No recording to save"));
        return false;
    }
    return true;
}

bool encodePcm16(const QByteArray& pcm, int rate, const wchar_t* dest, QString* error)
{
    if (pcm.size() < 2 || rate <= 0) {
        fail(error, QStringLiteral("No recording to save"));
        return false;
    }
    ComPtr<IMFSinkWriter> writer;
    HRESULT hr = MFCreateSinkWriterFromURL(dest, nullptr, nullptr, &writer.p);
    if (FAILED(hr) || !writer.p) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    ComPtr<IMFMediaType> outType;
    ComPtr<IMFMediaType> inType;
    if (FAILED(MFCreateMediaType(&outType.p)) || FAILED(MFCreateMediaType(&inType.p)) || !outType.p
        || !inType.p) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    const UINT32 hz = UINT32(rate);
    outType.p->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    outType.p->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_MP3);
    outType.p->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
    outType.p->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, hz);
    outType.p->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
    DWORD stream = 0;
    hr = writer.p->AddStream(outType.p, &stream);
    if (FAILED(hr)) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    inType.p->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    inType.p->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    inType.p->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
    inType.p->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, hz);
    inType.p->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    inType.p->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
    inType.p->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, hz * 2);
    hr = writer.p->SetInputMediaType(stream, inType.p, nullptr);
    if (FAILED(hr) || FAILED(writer.p->BeginWriting())) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    const int total = pcm.size() / 2;
    const int chunk = qMax(1, rate);
    int offset = 0;
    LONGLONG clock = 0;
    while (offset < total) {
        const int frames = qMin(chunk, total - offset);
        const DWORD bytes = DWORD(frames * 2);
        ComPtr<IMFSample> sample;
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(MFCreateSample(&sample.p)) || FAILED(MFCreateMemoryBuffer(bytes, &buffer.p))
            || !sample.p || !buffer.p) {
            fail(error, QStringLiteral("Could not save the MP3"));
            return false;
        }
        BYTE* data = nullptr;
        if (FAILED(buffer.p->Lock(&data, nullptr, nullptr)) || !data) {
            fail(error, QStringLiteral("Could not save the MP3"));
            return false;
        }
        memcpy(data, pcm.constData() + offset * 2, bytes);
        buffer.p->Unlock();
        buffer.p->SetCurrentLength(bytes);
        sample.p->AddBuffer(buffer.p);
        const LONGLONG duration = LONGLONG(frames) * 10000000LL / rate;
        sample.p->SetSampleTime(clock);
        sample.p->SetSampleDuration(duration);
        if (FAILED(writer.p->WriteSample(stream, sample.p))) {
            fail(error, QStringLiteral("Could not save the MP3"));
            return false;
        }
        clock += duration;
        offset += frames;
    }
    if (FAILED(writer.p->Finalize())) {
        fail(error, QStringLiteral("Could not save the MP3"));
        return false;
    }
    return true;
}

bool encodeMono(const QVector<float>& mono, int rate, const QString& dest, QString* error)
{
    const auto attempt = [&](int outRate) {
        const QVector<float> shaped = resample(mono, rate, outRate);
        const QByteArray pcm = toPcm16(shaped);
        const QString tmp = dest + QStringLiteral(".gazer-tmp.mp3");
        QFile::remove(tmp);
        const std::wstring wide = QFileInfo(tmp).absoluteFilePath().toStdWString();
        if (!encodePcm16(pcm, outRate, wide.c_str(), error)) {
            QFile::remove(tmp);
            return false;
        }
        return replaceFile(tmp, dest, error);
    };
    // The Windows MP3 encoder accepts common speech rates. 48 kHz is the fallback.
    if (rate > 0 && rate != kEncodeRate && attempt(rate)) {
        return true;
    }
    return attempt(kEncodeRate);
}

} // namespace

bool writeMp3File(const QString& source, const QString& dest, QString* error)
{
    if (source.isEmpty() || dest.isEmpty() || !QFileInfo::exists(source)) {
        fail(error, QStringLiteral("No recording to save"));
        return false;
    }
    if (QFileInfo(source).absoluteFilePath().compare(QFileInfo(dest).absoluteFilePath(),
                                                     Qt::CaseInsensitive)
        == 0) {
        return true;
    }
    const QString dir = QFileInfo(dest).absolutePath();
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
        fail(error, QStringLiteral("Could not use that folder"));
        return false;
    }
    if (QFileInfo(source).suffix().compare(QLatin1String("mp3"), Qt::CaseInsensitive) == 0) {
        return copyMp3(source, dest, error);
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(com);
    const HRESULT started = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    QVector<float> mono;
    int rate = 0;
    bool ok = SUCCEEDED(started) && readClip(source, &mono, &rate, error) && encodeMono(mono, rate, dest, error);
    if (SUCCEEDED(started)) {
        MFShutdown();
    }
    if (uninit) {
        CoUninitialize();
    }
    if (!ok && error && error->isEmpty()) {
        *error = QStringLiteral("Could not save the MP3");
    }
    return ok;
}

} // namespace gazer
