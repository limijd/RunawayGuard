#include "DaemonClient.h"
#include <QJsonDocument>
#include <unistd.h>

DaemonClient::DaemonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected, this, &DaemonClient::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DaemonClient::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DaemonClient::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DaemonClient::onError);
}

void DaemonClient::connectToDaemon()
{
    QString socketPath = QString("/run/user/%1/runaway-guard.sock").arg(getuid());
    m_socket->connectToServer(socketPath);
}

void DaemonClient::sendRequest(const QJsonObject &request)
{
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
}

void DaemonClient::onConnected() { emit connected(); }
void DaemonClient::onDisconnected() { emit disconnected(); }

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
            if (type == "alert") emit alertReceived(obj["data"].toObject());
            else if (type == "status") emit statusReceived(obj["data"].toObject());
            else emit responseReceived(obj);
        }
    }
}

void DaemonClient::onError(QLocalSocket::LocalSocketError error) { Q_UNUSED(error); }
