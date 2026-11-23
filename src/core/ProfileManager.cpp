#include "../../include/core/ProfileManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

ProfileManager::ProfileManager(QObject* parent) : QObject(parent) {}

bool ProfileManager::saveProfile(const QString& name, const QJsonObject& data, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(data);
    file.write(doc.toJson());
    currentProfileName_ = name;
    return true;
}

bool ProfileManager::loadProfile(const QString& path, QJsonObject& outData) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;
    outData = doc.object();
    return true;
}

QString ProfileManager::currentProfileName() const {
    return currentProfileName_;
}
