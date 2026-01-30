#ifndef DAEMONMANAGER_H
#define DAEMONMANAGER_H

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QVector>

class DaemonClient;

class DaemonManager : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Unknown,
        Starting,
        Running,
        Stopped,
        Failed
    };
    Q_ENUM(State)

    explicit DaemonManager(QObject *parent = nullptr);
    ~DaemonManager();

    void initialize();
    void shutdown();

    State state() const { return m_state; }
    DaemonClient* client() const { return m_client; }
    QString lastError() const { return m_lastError; }
    QString daemonBinaryPath() const { return m_daemonPath; }

    bool isDaemonRunning() const;

public slots:
    void startDaemon();
    void stopDaemon();
    void restartDaemon();

signals:
    void stateChanged(State state);
    void connected();
    void disconnected();
    void daemonStarted();
    void daemonStopped();
    void daemonCrashed();
    void errorOccurred(const QString &error);

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void tryConnect();
    void pollForSocket();

private:
    QString findDaemonBinary() const;
    void setState(State state);
    bool checkSocketExists() const;
    bool checkProcessRunning() const;
    void scheduleReconnect();
    void recordRestartAttempt();
    bool isInCrashLoop() const;
    QString socketPath() const;

    DaemonClient *m_client;
    QProcess *m_daemonProcess;
    QTimer *m_reconnectTimer;
    QTimer *m_socketPollTimer;

    State m_state;
    QString m_lastError;
    QString m_daemonPath;

    QVector<QDateTime> m_restartTimes;
    int m_reconnectAttempts;

    static const int CRASH_LOOP_THRESHOLD = 3;
    static const int CRASH_LOOP_WINDOW_SECS = 60;
    static const int MAX_RECONNECT_BEFORE_RESTART = 5;
    static const int RECONNECT_INTERVAL_MS = 2000;
    static const int SOCKET_POLL_INTERVAL_MS = 500;
    static const int STARTUP_TIMEOUT_MS = 10000;
};

#endif
