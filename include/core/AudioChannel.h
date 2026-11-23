#pragma once
#include <string>
#include <memory>

class AudioChannel {
public:
    explicit AudioChannel(std::string id, std::string name);
    virtual ~AudioChannel() = default;

    virtual void play(const std::string& filePath) = 0;
    virtual void stop() = 0;
    virtual void setVolume(float volume) = 0;
    virtual float getVolume() const = 0;
    virtual bool isPlaying() const = 0;

    const std::string& getId() const { return id_; }
    const std::string& getName() const { return name_; }

protected:
    std::string id_;
    std::string name_;
    float volume_ = 1.0f;
    bool playing_ = false;
};
