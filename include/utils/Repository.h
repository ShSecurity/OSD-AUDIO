#pragma once
#include <unordered_map>
#include <optional>
#include <vector>
#include <stdexcept>

template<typename T>
class Repository {
public:
    using IdType = std::string;

    void add(const IdType& id, T item) {
        if (data_.count(id)) throw std::runtime_error("ID already exists: " + id);
        data_[id] = std::move(item);
    }

    void update(const IdType& id, T item) {
        data_[id] = std::move(item);
    }

    void remove(const IdType& id) {
        data_.erase(id);
    }

    std::optional<T> find(const IdType& id) const {
        auto it = data_.find(id);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<T> getAll() const {
        std::vector<T> result;
        result.reserve(data_.size());
        for (const auto& [_, v] : data_) result.push_back(v);
        return result;
    }

    bool exists(const IdType& id) const { return data_.count(id) > 0; }
    size_t size() const { return data_.size(); }

private:
    std::unordered_map<IdType, T> data_;
};
