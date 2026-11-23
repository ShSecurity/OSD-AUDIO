#include "../../include/core/PriorityChannel.h"
#include <QThread>
#include <QUrl>
#include <algorithm>

PriorityChannel::PriorityChannel(const std::string& id,
                                  const std::string& name,
                                  int priority,
                                  QObject* parent)
    : QObject(parent)
    , AudioChannel(id, name)
    , priority_(priority)
{
    // QMediaPlayer y QAudioOutput deben vivir en el hilo principal Qt
    player_      = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOutput_);

    connect(player_, &QMediaPlayer::playbackStateChanged,
            this,    &PriorityChannel::onPlaybackStateChanged);
}

PriorityChannel::~PriorityChannel() = default;

// play() puede ser llamado desde el hilo trabajador de AudioMixer.
// Lo marshaleamos al hilo Qt via QueuedConnection si es necesario.
void PriorityChannel::play(const std::string& filePath) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "playSlot",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(filePath)));
        return;
    }
    playSlot(QString::fromStdString(filePath));
}

void PriorityChannel::playSlot(const QString& filePath) {
    if (filePath.isEmpty()) return;
    audioOutput_->setVolume(static_cast<float>(volume_));
    player_->setSource(QUrl::fromLocalFile(filePath));
    player_->play();
}

void PriorityChannel::stop() {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "stopSlot", Qt::QueuedConnection);
        return;
    }
    stopSlot();
}

void PriorityChannel::stopSlot() {
    player_->stop();
}

void PriorityChannel::setVolume(float volume) {
    volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (audioOutput_)
        audioOutput_->setVolume(static_cast<float>(volume_));
}

float PriorityChannel::getVolume() const {
    return volume_;
}

bool PriorityChannel::isPlaying() const {
    return player_->playbackState() == QMediaPlayer::PlayingState;
}

void PriorityChannel::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState && !playing_) {
        playing_ = true;
        emit playbackStarted(QString::fromStdString(id_));
    } else if (state == QMediaPlayer::StoppedState && playing_) {
        playing_ = false;
        emit playbackFinished(QString::fromStdString(id_));
    }
}
