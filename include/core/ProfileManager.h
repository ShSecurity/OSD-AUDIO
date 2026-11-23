#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

class ProfileManager : public QObject {
    Q_OBJECT
public:
    explicit ProfileManager(QObject* parent = nullptr);

    bool saveProfile(const QString& name, const QJsonObject& data, const QString& path);
    bool loadProfile(const QString& path, QJsonObject& outData);
    QString currentProfileName() const;

private:
    QString currentProfileName_;
};
