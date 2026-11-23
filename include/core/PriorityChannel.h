#pragma once
#include "AudioChannel.h"
#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QString>

// Canal de alta prioridad: alertas, TTS y efectos de sonido
// Herencia multiple: QObject (para signals/slots) + AudioChannel (polimorfismo C++)
class PriorityChannel : public QObject, public AudioChannel {
    Q_OBJECT
public:
    explicit PriorityChannel(const std::string& id,
                              const std::string& name,
                              int priority = 1,
                              QObject* parent = nullptr);
    ~PriorityChannel() override;

    // Implementacion de AudioChannel (virtual puro)
    void  play(const std::string& filePath) override;
    void  stop() override;
    void  setVolume(float volume) override;
    float getVolume() const override;
    bool  isPlaying() const override;

    int getPriority() const { return priority_; }

signals:
    void playbackStarted(const QString& channelId);
    void playbackFinished(const QString& channelId);

public slots:
    // Slot para ser invocado desde hilo trabajador via QMetaObject::invokeMethod
    void playSlot(const QString& filePath);
    void stopSlot();

private slots:
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    int           priority_;
    QMediaPlayer* player_      = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
};
