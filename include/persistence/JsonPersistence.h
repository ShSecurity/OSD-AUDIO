#pragma once
#include "../interfaces/IPersistence.h"
#include <QJsonObject>
#include <QString>

class JsonPersistence : public IPersistence<QJsonObject> {
public:
    bool save(const QJsonObject& data, const std::string& path) override;
    bool load(QJsonObject& data, const std::string& path) override;
};
