#pragma once
#include <QObject>
#include "AudioMixer.h"

class DuckingController : public QObject {
    Q_OBJECT
public:
    explicit DuckingController(AudioMixer* mixer, QObject* parent = nullptr);

public slots:
    void onPriorityChannelStarted();
    void onPriorityChannelFinished();

signals:
    void duckingActivated();
    void duckingDeactivated();

private:
    AudioMixer* mixer_;
    bool isDucking_ = false;
};
