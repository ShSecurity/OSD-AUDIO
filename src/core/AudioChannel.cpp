#include "../../include/core/AudioChannel.h"

AudioChannel::AudioChannel(std::string id, std::string name)
    : id_(std::move(id)), name_(std::move(name)) {}
