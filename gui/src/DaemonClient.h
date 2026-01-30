#ifndef DAEMONCLIENT_H
#define DAEMONCLIENT_H

#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QTimer>

class DaemonClient : public QObject
{
    Q_OBJECT

public:
    explicit DaemonClient(QObject *parent = nullptr);
    void connectToDaemon();
    void sendRequest(const QJsonObject &request);
    bool isConnected() const;

    // Convenience methods for common requests
    void requestProcessList();
    void requestAlerts(int limit = 50);
    void requestKillProcess(int pid, const QString &signal);
    void requestAddWhitelist(const QString &pattern, const QString &matchType);

signals:
    void connected();
    void disconnected();
    void alertReceived(const QJsonObject &alert);
    void statusReceived(const QJsonObject &status);
    void responseReceived(const QJsonObject &response);
    void processListReceived(const QJsonArray &processes);
    void alertListReceived(const QJsonArray &alerts);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);
    void tryReconnect();

private:
    QLocalSocket *m_socket;
    QByteArray m_buffer;
    QTimer *m_reconnectTimer;
    int m_reconnectAttempts;
    static const int MAX_RECONNECT_ATTEMPTS = 10;
    static const int RECONNECT_INTERVAL_MS = 3000;
};

#endif
