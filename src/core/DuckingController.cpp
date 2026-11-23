#include "../../include/core/DuckingController.h"

DuckingController::DuckingController(AudioMixer* mixer, QObject* parent)
    : QObject(parent), mixer_(mixer) {}

void DuckingController::onPriorityChannelStarted() {
    if (!isDucking_) {
        isDucking_ = true;
        emit duckingActivated();
    }
}

void DuckingController::onPriorityChannelFinished() {
    if (isDucking_) {
        isDucking_ = false;
        emit duckingDeactivated();
    }
}
