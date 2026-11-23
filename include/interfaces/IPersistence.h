#pragma once
#include <string>

template<typename T>
class IPersistence {
public:
    virtual ~IPersistence() = default;
    virtual bool save(const T& data, const std::string& path) = 0;
    virtual bool load(T& data, const std::string& path) = 0;
};
