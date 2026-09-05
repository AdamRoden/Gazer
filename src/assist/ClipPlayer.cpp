#include "assist/ClipPlayer.h"

#include "assist/AudioGain.h"
#include "assist/ClipBoost.h"
#include "utils/Log.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QTemporaryFile>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace {

double clampRate(double playbackRate)
{
    if (!std::isfinite(playbackRate)) {
        return 1.0;
    }
    return std::clamp(playbackRate, 0.25, 4.0);
}

} // namespace

ClipPlayer::ClipPlayer(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(1.0);
    m_boost = new ClipBoost(this);
    m_gainFile = new QTemporaryFile(this);
    m_gainFile->setAutoRemove(true);

    const auto outputs = QMediaDevices::audioOutputs();
    m_backendOk = m_player && m_audio && !outputs.isEmpty();
    if (!m_backendOk) {
        GAZER_WARN << "ClipPlayer: no audio output device or QMediaPlayer backend";
    }

    connect(m_boost, &ClipBoost::finished, this, &ClipPlayer::onBoostFinished);
    connect(m_boost, &ClipBoost::failed, this, &ClipPlayer::onBoostFailed);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState) {
        onStateChanged();
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString&) {
        onError();
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus) {
        onMediaStatus();
    });
}

ClipPlayer::~ClipPlayer()
{
    stop();
}

bool ClipPlayer::isAvailable() const
{
    return m_backendOk;
}

void ClipPlayer::stopQuiet()
{
    m_boosting = false;
    m_boost->cancel();
    m_suppressSignals = true;
    m_armed = false;
    if (m_player && m_player->playbackState() != QMediaPlayer::StoppedState) {
        m_player->stop();
    }
    if (m_player) {
        m_player->setSource(QUrl());
    }
    if (m_gainFile->isOpen()) {
        m_gainFile->close();
    }
    m_playing = false;
    m_suppressSignals = false;
}

bool ClipPlayer::play(const QString& path, double playbackRate, double gain)
{
    if (!m_backendOk || !m_player || !m_audio) {
        return false;
    }
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile()) {
        return false;
    }

    stopQuiet();
    m_sourcePath = info.absoluteFilePath();
    m_rate = clampRate(playbackRate);
    const double g = AudioGain::clamp(gain);
    if (!AudioGain::needsDecode(g) || !m_boost->isSupported()) {
        return startFile(m_sourcePath, m_rate);
    }

    m_boosting = true;
    m_boost->start(m_sourcePath, g);
    return true;
}

bool ClipPlayer::startFile(const QString& path, double playbackRate)
{
    m_audio->setVolume(1.0);
    m_player->setPlaybackRate(playbackRate);
    m_armed = true;
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
    if (m_player->error() != QMediaPlayer::NoError
        || m_player->mediaStatus() == QMediaPlayer::InvalidMedia) {
        m_armed = false;
        return false;
    }
    return true;
}

void ClipPlayer::playUnboostedOrFail()
{
    m_boosting = false;
    if (m_sourcePath.isEmpty() || !startFile(m_sourcePath, m_rate)) {
        emit failed(QStringLiteral("Clip play failed"));
    }
}

void ClipPlayer::onBoostFinished(const QByteArray& wav)
{
    if (!m_boosting) {
        return;
    }
    m_boosting = false;
    if (m_gainFile->isOpen()) {
        m_gainFile->close();
    }
    if (!m_gainFile->open() || m_gainFile->write(wav) != wav.size()) {
        playUnboostedOrFail();
        return;
    }
    m_gainFile->flush();
    m_gainFile->close();
    if (!startFile(m_gainFile->fileName(), m_rate)) {
        playUnboostedOrFail();
    }
}

void ClipPlayer::onBoostFailed()
{
    if (!m_boosting) {
        return;
    }
    playUnboostedOrFail();
}

void ClipPlayer::stop()
{
    const bool wasPlaying = m_playing;
    stopQuiet();
    if (wasPlaying) {
        emit stopped();
    }
}

bool ClipPlayer::isPlaying() const
{
    return m_playing;
}

void ClipPlayer::onStateChanged()
{
    if (!m_player || m_suppressSignals) {
        return;
    }
    const auto st = m_player->playbackState();
    if (st == QMediaPlayer::PlayingState) {
        m_armed = false;
        if (!m_playing) {
            m_playing = true;
            emit started();
        }
        return;
    }
    if (st == QMediaPlayer::StoppedState && m_player->source().isValid()) {
        m_suppressSignals = true;
        m_player->setSource(QUrl());
        m_suppressSignals = false;
    }
    if (m_playing) {
        m_playing = false;
        emit stopped();
    }
}

void ClipPlayer::onMediaStatus()
{
    if (!m_player || m_suppressSignals) {
        return;
    }
    const auto st = m_player->mediaStatus();
    if (st == QMediaPlayer::InvalidMedia && m_armed) {
        m_armed = false;
        const QString err = m_player->errorString().isEmpty() ? QStringLiteral("Invalid media")
                                                              : m_player->errorString();
        emit failed(err);
        if (m_playing) {
            m_playing = false;
            emit stopped();
        }
        return;
    }
    if (st == QMediaPlayer::EndOfMedia) {
        const bool requested = m_armed || m_playing;
        m_armed = false;
        if (m_playing || m_player->playbackState() != QMediaPlayer::StoppedState) {
            m_player->stop();
            return;
        }
        if (requested) {
            emit stopped();
        }
    }
}

void ClipPlayer::onError()
{
    if (!m_player || m_suppressSignals) {
        return;
    }
    m_armed = false;
    const QString err = m_player->errorString().isEmpty() ? QStringLiteral("Clip play failed")
                                                          : m_player->errorString();
    GAZER_WARN << "ClipPlayer:" << err;
    emit failed(err);
    if (m_playing) {
        m_playing = false;
        emit stopped();
    }
}

} // namespace gazer
