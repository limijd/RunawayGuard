#include "DaemonClient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <unistd.h>

DaemonClient::DaemonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
{
    connect(m_socket, &QLocalSocket::connected, this, &DaemonClient::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DaemonClient::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DaemonClient::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DaemonClient::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DaemonClient::tryReconnect);
}

void DaemonClient::connectToDaemon()
{
    QString socketPath = QString("/run/user/%1/runaway-guard.sock").arg(getuid());
    m_socket->connectToServer(socketPath);
}

bool DaemonClient::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState;
}

void DaemonClient::sendRequest(const QJsonObject &request)
{
    if (!isConnected()) return;
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
}

void DaemonClient::requestProcessList()
{
    sendRequest(QJsonObject{{"cmd", "list_processes"}});
}

void DaemonClient::requestAlerts(int limit)
{
    QJsonObject params{{"limit", limit}};
    sendRequest(QJsonObject{{"cmd", "get_alerts"}, {"params", params}});
}

void DaemonClient::requestKillProcess(int pid, const QString &signal)
{
    QJsonObject params{{"pid", pid}, {"signal", signal}};
    sendRequest(QJsonObject{{"cmd", "kill_process"}, {"params", params}});
}

void DaemonClient::requestWhitelist()
{
    sendRequest(QJsonObject{{"cmd", "list_whitelist"}});
}

void DaemonClient::requestAddWhitelist(const QString &pattern, const QString &matchType)
{
    QJsonObject params{{"pattern", pattern}, {"match_type", matchType}};
    sendRequest(QJsonObject{{"cmd", "add_whitelist"}, {"params", params}});
    // Refresh whitelist after adding
    requestWhitelist();
}

void DaemonClient::requestRemoveWhitelist(int id)
{
    QJsonObject params{{"id", id}};
    sendRequest(QJsonObject{{"cmd", "remove_whitelist"}, {"params", params}});
    // Refresh whitelist after removing
    requestWhitelist();
}

void DaemonClient::onConnected()
{
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
    emit connected();
}

void DaemonClient::onDisconnected()
{
    emit disconnected();
    // Start reconnection attempts
    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
    }
}

void DaemonClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    while (true) {
        int newlinePos = m_buffer.indexOf('\n');
        if (newlinePos < 0) break;
        QByteArray line = m_buffer.left(newlinePos);
        m_buffer.remove(0, newlinePos + 1);
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();
            if (type == "alert") {
                emit alertReceived(obj["data"].toObject());
            } else if (type == "status") {
                emit statusReceived(obj["data"].toObject());
            } else if (type == "response") {
                QJsonValue data = obj["data"];
                // Check if it's a process list or alert list response
                if (data.isArray()) {
                    QJsonArray arr = data.toArray();
                    if (!arr.isEmpty()) {
                        QJsonObject first = arr[0].toObject();
                        if (first.contains("cpu_percent")) {
                            emit processListReceived(arr);
                        } else if (first.contains("reason")) {
                            emit alertListReceived(arr);
                        } else if (first.contains("pattern") && first.contains("match_type")) {
                            emit whitelistReceived(arr);
                        }
                    } else {
                        // Empty array - could be empty whitelist
                        QString cmd = obj["cmd"].toString();
                        if (cmd == "list_whitelist") {
                            emit whitelistReceived(arr);
                        }
                    }
                }
                emit responseReceived(obj);
            } else if (type == "pong") {
                emit responseReceived(obj);
            }
        }
    }
}

void DaemonClient::onError(QLocalSocket::LocalSocketError error)
{
    Q_UNUSED(error);
    // Try to reconnect on error
    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
    }
}

void DaemonClient::tryReconnect()
{
    m_reconnectAttempts++;
    if (m_reconnectAttempts <= MAX_RECONNECT_ATTEMPTS) {
        connectToDaemon();
    } else {
        m_reconnectTimer->stop();
    }
}
