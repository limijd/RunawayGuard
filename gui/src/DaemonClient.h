#ifndef DAEMONCLIENT_H
#define DAEMONCLIENT_H

#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>

class DaemonClient : public QObject
{
    Q_OBJECT

public:
    explicit DaemonClient(QObject *parent = nullptr);
    void connectToDaemon();
    void sendRequest(const QJsonObject &request);

signals:
    void connected();
    void disconnected();
    void alertReceived(const QJsonObject &alert);
    void statusReceived(const QJsonObject &status);
    void responseReceived(const QJsonObject &response);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);

private:
    QLocalSocket *m_socket;
    QByteArray m_buffer;
};

#endif
