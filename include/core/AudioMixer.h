#pragma once
#include "AudioChannel.h"
#include "PriorityChannel.h"
#include "BackgroundChannel.h"
#include "../utils/ThreadSafeQueue.h"
#include <QObject>
#include <QString>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

struct AudioRequest {
    std::string channelId;
    std::string filePath;
    float volume = 1.0f;
};

class AudioMixer : public QObject {
    Q_OBJECT
public:
    explicit AudioMixer(QObject* parent = nullptr);
    ~AudioMixer();

    void addPriorityChannel(std::shared_ptr<PriorityChannel> channel);
    void addBackgroundChannel(std::shared_ptr<BackgroundChannel> channel);
    void removeChannel(const std::string& id);

    void enqueuePlay(AudioRequest request);
    void stopChannel(const std::string& channelId);
    void stopAll();

    void  setMasterVolume(float volume);
    float getMasterVolume() const;

    // Ducking manual
    void activateDucking(float targetVol = 0.2f, int fadeMs = 300);
    void deactivateDucking(int fadeMs = 300);

signals:
    void playbackStarted(const QString& channelId);
    void playbackFinished(const QString& channelId);

private:
    void processQueue();
    void applyDucking(bool activate);

    std::vector<std::shared_ptr<PriorityChannel>>   priorityChannels_;
    std::vector<std::shared_ptr<BackgroundChannel>> backgroundChannels_;
    ThreadSafeQueue<AudioRequest>                   requestQueue_;

    std::mutex        mixerMutex_;
    std::thread       workerThread_;
    std::atomic<bool> running_;
    float             masterVolume_ = 1.0f;
    float             duckTarget_   = 0.2f;
};
