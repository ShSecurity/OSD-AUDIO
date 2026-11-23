#pragma once
#include <string>
#include <functional>

enum class StreamEventType {
    Donation,
    Comment,
    Subscription,
    Alert,
    Custom
};

struct StreamEvent {
    StreamEventType type;
    std::string username;
    std::string message;
    double amount = 0.0;
};

class IStreamAdapter {
public:
    virtual ~IStreamAdapter() = default;
    virtual void connect(const std::string& url) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual void setEventCallback(std::function<void(const StreamEvent&)> callback) = 0;
};
