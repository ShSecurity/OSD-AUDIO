#include "TikTokBridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QFileInfo>

TikTokBridge::TikTokBridge(QObject* parent) : QObject(parent) {
    process_ = new QProcess(this);
    process_->setReadChannel(QProcess::StandardOutput);

    connect(process_, &QProcess::started,
            this, &TikTokBridge::onProcessStarted);
    connect(process_, &QProcess::readyReadStandardOutput,
            this, &TikTokBridge::onProcessReadyRead);
    // Descartar stderr — solo muestra eventos limpios al usuario
    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        process_->readAllStandardError();  // leer y tirar
    });
    connect(process_, &QProcess::errorOccurred,
            this, &TikTokBridge::onProcessError);
    connect(process_, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TikTokBridge::onProcessFinished);

    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setSingleShot(true);
    connect(reconnectTimer_, &QTimer::timeout, this, &TikTokBridge::onReconnectTimer);
}

TikTokBridge::~TikTokBridge() { disconnectFromServer(); }

void TikTokBridge::connectToServer(const QString& username) {
    userDisconnected_  = false;
    reconnectAttempt_  = 0;
    username_ = username.startsWith('@') ? username : '@' + username;

    if (process_->state() != QProcess::NotRunning) process_->kill();

    QString exeDir = QCoreApplication::applicationDirPath();
    QString script = exeDir + "/stream_bridge.py";
    if (!QFileInfo::exists(script))
        script = exeDir + "/../stream_bridge.py";

    process_->start("python", {script, username_});
}

void TikTokBridge::disconnectFromServer() {
    userDisconnected_ = true;
    reconnectTimer_->stop();
    if (process_->state() != QProcess::NotRunning) {
        process_->terminate();
        if (!process_->waitForFinished(2000)) {
            process_->kill();
            process_->waitForFinished(1000);
        }
    }
    if (connected_) { connected_ = false; emit disconnected(); }
}

void TikTokBridge::scheduleReconnect() {
    if (userDisconnected_ || reconnectAttempt_ >= MAX_RECONNECT) return;
    ++reconnectAttempt_;
    emit reconnecting(reconnectAttempt_, MAX_RECONNECT);
    reconnectTimer_->start(4000 * reconnectAttempt_);  // 4s, 8s, 12s…
}

void TikTokBridge::onReconnectTimer() {
    if (userDisconnected_) return;
    if (process_->state() != QProcess::NotRunning) process_->kill();
    QString exeDir = QCoreApplication::applicationDirPath();
    QString script = exeDir + "/stream_bridge.py";
    if (!QFileInfo::exists(script)) script = exeDir + "/../stream_bridge.py";
    process_->start("python", {script, username_});
}

void TikTokBridge::onProcessStarted() {
    // No emitir connected todavía: esperamos el JSON {"type":"connected"} de Python
    reconnectAttempt_ = 0;
}

void TikTokBridge::onProcessReadyRead() {
    while (process_->canReadLine()) {
        QString line = QString::fromUtf8(process_->readLine()).trimmed();
        if (!line.isEmpty()) parseEvent(line);
    }
}

void TikTokBridge::onProcessError(QProcess::ProcessError error) {
    connected_ = false;
    if (error == QProcess::FailedToStart) {
        emit connectionError("No se encontró Python. ¿Está instalado?");
        return;
    }
    scheduleReconnect();
}

void TikTokBridge::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    bool wasConnected = connected_;
    connected_ = false;

    if (userDisconnected_) {
        if (wasConnected) emit disconnected();
        return;
    }

    if (!wasConnected) {
        // Proceso terminó sin haber confirmado conexión con TikTok
        // (live no encontrado, error de red, etc.) — resetear UI sin mensaje de error
        emit connectionError("No se pudo establecer conexión. ¿Estás en vivo?");
        return;
    }

    // Estaba conectado y se cayó
    if (exitCode == 0) {
        emit disconnected();      // desconexión limpia (live terminó)
    } else {
        scheduleReconnect();      // caída inesperada → reconectar
    }
}

void TikTokBridge::parseEvent(const QString& jsonLine) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonLine.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject obj  = doc.object();
    QString     type = obj.value("type").toString();

    if      (type == "connected") {
        connected_ = true;
        emit connected(obj.value("username").toString());
    }
    else if (type == "disconnected") {
        if (connected_) { connected_ = false; emit disconnected(); }
    }
    else if (type == "comment")  emit commentReceived(obj.value("username").toString(), obj.value("text").toString());
    else if (type == "gift")     emit giftReceived(obj.value("username").toString(), obj.value("amount").toInt(), obj.value("gift_name").toString());
    else if (type == "follow")   emit followReceived(obj.value("username").toString());
    else if (type == "like")     emit likeReceived(obj.value("username").toString(), obj.value("count").toInt());
    else if (type == "live_not_found") {
        // El live no está activo — no reconectar automáticamente
        userDisconnected_ = true;
        connected_ = false;
        emit connectionError(obj.value("message").toString());
    }
    else if (type == "error")    emit connectionError(obj.value("message").toString());
}
