#include "../../include/core/BackgroundChannel.h"
#include <QThread>
#include <QUrl>
#include <algorithm>
#include <cmath>

BackgroundChannel::BackgroundChannel(const std::string& id,
                                      const std::string& name,
                                      QObject* parent)
    : QObject(parent)
    , AudioChannel(id, name)
{
    player_      = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOutput_);
    player_->setLoops(QMediaPlayer::Infinite);  // Loop de fondo infinito

    fadeTimer_ = new QTimer(this);
    fadeTimer_->setInterval(20);  // Fade cada 20 ms (~50 fps)
    connect(fadeTimer_, &QTimer::timeout,
            this,       &BackgroundChannel::onFadeTimer);
    connect(player_, &QMediaPlayer::playbackStateChanged,
            this,    &BackgroundChannel::onPlaybackStateChanged);
}

BackgroundChannel::~BackgroundChannel() = default;

void BackgroundChannel::play(const std::string& filePath) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "playSlot",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(filePath)));
        return;
    }
    playSlot(QString::fromStdString(filePath));
}

void BackgroundChannel::playSlot(const QString& filePath) {
    if (filePath.isEmpty()) return;
    audioOutput_->setVolume(static_cast<float>(volume_));
    player_->setSource(QUrl::fromLocalFile(filePath));
    player_->play();
    playing_ = true;
    emit playbackStarted(QString::fromStdString(id_));
}

void BackgroundChannel::stop() {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "stopSlot", Qt::QueuedConnection);
        return;
    }
    stopSlot();
}

void BackgroundChannel::stopSlot() {
    fadeTimer_->stop();
    player_->stop();
    playing_  = false;
    isDucked_ = false;
    emit playbackFinished(QString::fromStdString(id_));
}

void BackgroundChannel::setVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (!isDucked_ && audioOutput_)
        audioOutput_->setVolume(static_cast<float>(volume_));
}

float BackgroundChannel::getVolume() const {
    return volume_;
}

bool BackgroundChannel::isPlaying() const {
    return player_->playbackState() == QMediaPlayer::PlayingState;
}

// Inicia un fade gradual hacia targetVolume en durationMs milisegundos
void BackgroundChannel::duck(float targetVolume, int fadeDurationMs) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, targetVolume, fadeDurationMs]() {
            duck(targetVolume, fadeDurationMs);
        }, Qt::QueuedConnection);
        return;
    }
    isDucked_      = true;
    duckedVolume_  = std::clamp(targetVolume, 0.0f, 1.0f);
    startFade(duckedVolume_, fadeDurationMs);
}

void BackgroundChannel::unduck(int fadeDurationMs) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, fadeDurationMs]() {
            unduck(fadeDurationMs);
        }, Qt::QueuedConnection);
        return;
    }
    isDucked_ = false;
    startFade(volume_, fadeDurationMs);
}

void BackgroundChannel::startFade(float targetVol, int durationMs) {
    fadeTimer_->stop();
    fadeTarget_ = targetVol;
    float current = audioOutput_->volume();
    int steps     = std::max(1, durationMs / 20);
    fadeStep_     = (current - targetVol) / static_cast<float>(steps);
    if (std::abs(fadeStep_) < 0.0001f) {
        audioOutput_->setVolume(targetVol);
        return;
    }
    fadeTimer_->start();
}

void BackgroundChannel::onFadeTimer() {
    float current = audioOutput_->volume();
    float next    = current - fadeStep_;

    // Verificar si llegamos al objetivo
    bool done = (fadeStep_ > 0.0f) ? (next <= fadeTarget_)
                                    : (next >= fadeTarget_);
    if (done) {
        audioOutput_->setVolume(fadeTarget_);
        fadeTimer_->stop();
        return;
    }
    audioOutput_->setVolume(next);
}

void BackgroundChannel::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::StoppedState && playing_) {
        playing_ = false;
        emit playbackFinished(QString::fromStdString(id_));
    }
}
