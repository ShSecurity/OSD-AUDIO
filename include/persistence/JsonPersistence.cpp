#include "JsonPersistence.h"
#include <QFile>
#include <QJsonDocument>

bool JsonPersistence::save(const QJsonObject& data, const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(data).toJson());
    return true;
}

bool JsonPersistence::load(QJsonObject& data, const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;
    data = doc.object();
    return true;
}
