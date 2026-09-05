#include "assist/ClipPlayer.h"

#include "utils/Log.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace gazer {

ClipPlayer::ClipPlayer(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);

    const auto outputs = QMediaDevices::audioOutputs();
    m_backendOk = m_player && m_audio && !outputs.isEmpty();
    if (!m_backendOk) {
        GAZER_WARN << "ClipPlayer: no audio output device or QMediaPlayer backend";
    }

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

bool ClipPlayer::play(const QString& path, double playbackRate)
{
    if (!m_backendOk || !m_player || !m_audio) {
        return false;
    }
    const QFileInfo info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile()) {
        return false;
    }

    m_suppressSignals = true;
    m_armed = false;
    if (m_player->playbackState() != QMediaPlayer::StoppedState) {
        m_player->stop();
    }
    m_playing = false;
    m_player->setSource(QUrl());
    m_suppressSignals = false;

    double rate = playbackRate;
    if (!std::isfinite(rate)) {
        rate = 1.0;
    }
    rate = std::clamp(rate, 0.25, 4.0);
    m_player->setPlaybackRate(rate);
    m_armed = true;
    m_player->setSource(QUrl::fromLocalFile(info.absoluteFilePath()));
    m_player->play();
    if (m_player->error() != QMediaPlayer::NoError
        || m_player->mediaStatus() == QMediaPlayer::InvalidMedia) {
        m_armed = false;
        return false;
    }
    return true;
}

void ClipPlayer::stop()
{
    if (!m_player) {
        return;
    }
    m_armed = false;
    m_suppressSignals = true;
    if (m_player->playbackState() != QMediaPlayer::StoppedState) {
        m_player->stop();
    }
    m_player->setSource(QUrl());
    m_suppressSignals = false;
    if (m_playing) {
        m_playing = false;
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
