#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>

class TikTokBridge : public QObject {
    Q_OBJECT
public:
    explicit TikTokBridge(QObject* parent = nullptr);
    ~TikTokBridge() override;

    void connectToServer(const QString& username);
    void disconnectFromServer();
    bool isConnected() const { return connected_; }
    QString serverUrl() const { return username_; }

signals:
    void connected(const QString& username);
    void disconnected();
    void reconnecting(int attempt, int maxAttempts);  // intento de re-conexion
    void connectionError(const QString& message);

    void commentReceived(const QString& username, const QString& text);
    void giftReceived(const QString& username, int amount, const QString& giftName);
    void followReceived(const QString& username);
    void likeReceived(const QString& username, int count);

private slots:
    void onProcessStarted();
    void onProcessReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onReconnectTimer();

private:
    void parseEvent(const QString& jsonLine);
    void scheduleReconnect();

    QProcess* process_          = nullptr;
    QTimer*   reconnectTimer_   = nullptr;
    QString   username_;
    bool      connected_        = false;
    bool      userDisconnected_ = false;
    int       reconnectAttempt_ = 0;
    static const int MAX_RECONNECT = 5;
};
