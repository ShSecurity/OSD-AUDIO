#include "../../include/core/AudioMixer.h"
#include <algorithm>

AudioMixer::AudioMixer(QObject* parent) : QObject(parent), running_(true) {
    // Canales por defecto disponibles desde el inicio
    auto defPriority = std::make_shared<PriorityChannel>("priority_1", "Default Priority", 1);
    auto defBg       = std::make_shared<BackgroundChannel>("background_1", "Default Background");
    addPriorityChannel(defPriority);
    addBackgroundChannel(defBg);

    // Conectar senal de fin de reproduccion del canal priority
    connect(defPriority.get(), &PriorityChannel::playbackFinished,
            this,              &AudioMixer::playbackFinished);
    connect(defPriority.get(), &PriorityChannel::playbackStarted,
            this,              &AudioMixer::playbackStarted);

    // Hilo trabajador: procesa cola de solicitudes de audio
    workerThread_ = std::thread(&AudioMixer::processQueue, this);
}

AudioMixer::~AudioMixer() {
    running_ = false;
    requestQueue_.stop();
    if (workerThread_.joinable())
        workerThread_.join();
}

void AudioMixer::addPriorityChannel(std::shared_ptr<PriorityChannel> channel) {
    std::lock_guard<std::mutex> lock(mixerMutex_);
    priorityChannels_.push_back(std::move(channel));
}

void AudioMixer::addBackgroundChannel(std::shared_ptr<BackgroundChannel> channel) {
    std::lock_guard<std::mutex> lock(mixerMutex_);
    backgroundChannels_.push_back(std::move(channel));
}

void AudioMixer::removeChannel(const std::string& id) {
    std::lock_guard<std::mutex> lock(mixerMutex_);
    auto pred = [&id](const auto& ch){ return ch->getId() == id; };
    priorityChannels_.erase(
        std::remove_if(priorityChannels_.begin(), priorityChannels_.end(), pred),
        priorityChannels_.end());
    backgroundChannels_.erase(
        std::remove_if(backgroundChannels_.begin(), backgroundChannels_.end(), pred),
        backgroundChannels_.end());
}

void AudioMixer::enqueuePlay(AudioRequest request) {
    requestQueue_.push(std::move(request));
}

void AudioMixer::stopAll() {
    std::vector<std::shared_ptr<AudioChannel>> channels;
    {
        std::lock_guard<std::mutex> lock(mixerMutex_);
        channels.insert(channels.end(), priorityChannels_.begin(), priorityChannels_.end());
        channels.insert(channels.end(), backgroundChannels_.begin(), backgroundChannels_.end());
    }
    // La señal de fin puede llamar al mezclador de nuevo desde la interfaz.
    for (const auto& channel : channels) channel->stop();
}

void AudioMixer::stopChannel(const std::string& channelId) {
    std::lock_guard<std::mutex> lock(mixerMutex_);
    for (auto& ch : priorityChannels_) {
        if (ch->getId() == channelId) { ch->stop(); return; }
    }
    for (auto& ch : backgroundChannels_) {
        if (ch->getId() == channelId) { ch->stop(); return; }
    }
}

void AudioMixer::setMasterVolume(float volume) {
    masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
    std::lock_guard<std::mutex> lock(mixerMutex_);
    for (auto& ch : priorityChannels_)
        ch->setVolume(masterVolume_);
    for (auto& ch : backgroundChannels_)
        if (!ch->isDucked())
            ch->setVolume(masterVolume_);
}

float AudioMixer::getMasterVolume() const {
    return masterVolume_;
}

void AudioMixer::activateDucking(float targetVol, int fadeMs) {
    duckTarget_ = targetVol;
    applyDucking(true);
}

void AudioMixer::deactivateDucking(int fadeMs) {
    applyDucking(false);
}

// Hilo trabajador: espera solicitudes y las despacha al hilo Qt via invokeMethod
void AudioMixer::processQueue() {
    while (running_) {
        try {
            auto req = requestQueue_.waitAndPop();
            std::lock_guard<std::mutex> lock(mixerMutex_);

            // Buscar y reproducir en canal de prioridad
            for (auto& ch : priorityChannels_) {
                if (ch->getId() == req.channelId) {
                    ch->setVolume(req.volume * masterVolume_);
                    // Marshall al hilo principal Qt (QueuedConnection)
                    QMetaObject::invokeMethod(ch.get(), "playSlot",
                        Qt::QueuedConnection,
                        Q_ARG(QString, QString::fromStdString(req.filePath)));
                    // Activar ducking en canales de fondo
                    for (auto& bg : backgroundChannels_)
                        QMetaObject::invokeMethod(bg.get(),
                            [bgPtr = bg.get(), this]() {
                                bgPtr->duck(duckTarget_, 300);
                            }, Qt::QueuedConnection);
                    break;
                }
            }

            // Buscar en canal de fondo
            for (auto& ch : backgroundChannels_) {
                if (ch->getId() == req.channelId) {
                    ch->setVolume(req.volume * masterVolume_);
                    QMetaObject::invokeMethod(ch.get(), "playSlot",
                        Qt::QueuedConnection,
                        Q_ARG(QString, QString::fromStdString(req.filePath)));
                    break;
                }
            }
        } catch (...) {
            break;
        }
    }
}

void AudioMixer::applyDucking(bool activate) {
    std::lock_guard<std::mutex> lock(mixerMutex_);
    for (auto& bg : backgroundChannels_) {
        if (activate)
            bg->duck(duckTarget_, 300);
        else
            bg->unduck(300);
    }
}
