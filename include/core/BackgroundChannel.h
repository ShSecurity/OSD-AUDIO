#pragma once
#include "AudioChannel.h"
#include <QObject>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QString>

// Canal de musica de fondo: atenuable dinamicamente via ducking con fade suave
class BackgroundChannel : public QObject, public AudioChannel {
    Q_OBJECT
public:
    explicit BackgroundChannel(const std::string& id,
                                const std::string& name,
                                QObject* parent = nullptr);
    ~BackgroundChannel() override;

    // Implementacion de AudioChannel (virtual puro)
    void  play(const std::string& filePath) override;
    void  stop() override;
    void  setVolume(float volume) override;
    float getVolume() const override;
    bool  isPlaying() const override;

    // Ducking: atenua el canal gradualmente
    void  duck(float targetVolume, int fadeDurationMs);
    void  unduck(int fadeDurationMs);
    float getDuckedVolume() const { return duckedVolume_; }
    bool  isDucked() const { return isDucked_; }

signals:
    void playbackStarted(const QString& channelId);
    void playbackFinished(const QString& channelId);

public slots:
    void playSlot(const QString& filePath);
    void stopSlot();

private slots:
    void onFadeTimer();
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    void startFade(float targetVol, int durationMs);

    float         duckedVolume_  = 1.0f;
    bool          isDucked_      = false;
    float         fadeTarget_    = 1.0f;
    float         fadeStep_      = 0.0f;
    QMediaPlayer* player_        = nullptr;
    QAudioOutput* audioOutput_   = nullptr;
    QTimer*       fadeTimer_     = nullptr;
};
