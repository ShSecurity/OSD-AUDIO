#pragma once

#include <QMap>
#include <QString>
#include <memory>

class QSettings;

class ApplicationDucking {
public:
    explicit ApplicationDucking(QSettings& settings);
    ~ApplicationDucking();

    QMap<QString, QString> refresh();
    void setActive(bool active);
    void setGlobalPercent(int percent);
    int applicationPercent(const QString& key) const;
    void setApplicationPercent(const QString& key, int percent);

private:
    struct State;
    std::unique_ptr<State> state_;
    QSettings& settings_;
    bool active_ = false;
    int globalPercent_ = 20;
};
