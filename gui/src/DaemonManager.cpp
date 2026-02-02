#include "DaemonManager.h"
#include "DaemonClient.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <unistd.h>

DaemonManager::DaemonManager(QObject *parent)
    : QObject(parent)
    , m_client(new DaemonClient(this))
    , m_daemonProcess(nullptr)
    , m_reconnectTimer(new QTimer(this))
    , m_socketPollTimer(new QTimer(this))
    , m_state(State::Unknown)
    , m_reconnectAttempts(0)
    , m_manageDaemonLifecycle(true)
{
    // Disable DaemonClient's internal auto-reconnect - we manage it
    m_client->setAutoReconnect(false);

    // Forward client signals
    connect(m_client, &DaemonClient::connected, this, &DaemonManager::onClientConnected);
    connect(m_client, &DaemonClient::disconnected, this, &DaemonManager::onClientDisconnected);

    // Setup reconnect timer
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DaemonManager::tryConnect);

    // Setup socket poll timer (for waiting after daemon start)
    m_socketPollTimer->setInterval(SOCKET_POLL_INTERVAL_MS);
    connect(m_socketPollTimer, &QTimer::timeout, this, &DaemonManager::pollForSocket);

    // Find daemon binary at construction
    m_daemonPath = findDaemonBinary();
}

DaemonManager::~DaemonManager()
{
    shutdown();
}

void DaemonManager::initialize()
{
    if (m_daemonPath.isEmpty()) {
        m_lastError = tr("Daemon binary not found. Please install runaway-daemon.");
        setState(State::Failed);
        emit errorOccurred(m_lastError);
        return;
    }

    // Try to connect first (daemon might already be running)
    tryConnect();
}

void DaemonManager::shutdown()
{
    m_reconnectTimer->stop();
    m_socketPollTimer->stop();

    // Read the latest setting from QSettings (user may have changed it during session)
    QSettings settings("RunawayGuard", "GUI");
    bool manageDaemonLifecycle = settings.value("manageDaemonLifecycle", true).toBool();

    // Stop daemon if lifecycle management is enabled
    if (manageDaemonLifecycle) {
        if (m_daemonProcess && m_daemonProcess->state() == QProcess::Running) {
            m_daemonProcess->terminate();
            if (!m_daemonProcess->waitForFinished(3000)) {
                m_daemonProcess->kill();
                m_daemonProcess->waitForFinished(1000);
            }
        }
    }
}

void DaemonManager::setManageDaemonLifecycle(bool manage)
{
    m_manageDaemonLifecycle = manage;
}

QString DaemonManager::findDaemonBinary() const
{
    // 1. Environment override (for development/testing)
    QString envPath = qEnvironmentVariable("RUNAWAY_DAEMON_PATH");
    if (!envPath.isEmpty() && QFile::exists(envPath)) {
        return envPath;
    }

    // 2. Adjacent to GUI binary (bundled installation)
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList bundledPaths = {
        appDir + "/../libexec/runaway-daemon",
        appDir + "/runaway-daemon",
        appDir + "/../bin/runaway-daemon"
    };
    for (const auto &path : bundledPaths) {
        if (QFile::exists(path)) {
            return QDir::cleanPath(path);
        }
    }

    // 3. Standard system paths
    const QStringList standardPaths = {
        "/usr/local/bin/runaway-daemon",
        "/usr/bin/runaway-daemon"
    };
    for (const auto &path : standardPaths) {
        if (QFile::exists(path)) {
            return path;
        }
    }

    // 4. PATH search
    QString pathResult = QStandardPaths::findExecutable("runaway-daemon");
    if (!pathResult.isEmpty()) {
        return pathResult;
    }

    // 5. Development paths (relative to GUI build directory)
    QStringList devPaths = {
        appDir + "/../../daemon/target/release/runaway-daemon",
        appDir + "/../../../daemon/target/release/runaway-daemon",
        appDir + "/../../.worktrees/dev/daemon/target/release/runaway-daemon",
        appDir + "/../../../.worktrees/dev/daemon/target/release/runaway-daemon"
    };
    for (const auto &devPath : devPaths) {
        if (QFile::exists(devPath)) {
            return QDir::cleanPath(devPath);
        }
    }

    return QString();
}

QString DaemonManager::socketPath() const
{
    return QString("/run/user/%1/runaway-guard.sock").arg(getuid());
}

bool DaemonManager::checkSocketExists() const
{
    return QFile::exists(socketPath());
}

bool DaemonManager::checkProcessRunning() const
{
    // Check our managed process first
    if (m_daemonProcess && m_daemonProcess->state() == QProcess::Running) {
        return true;
    }

    // Also check if any runaway-daemon process exists (could be externally started)
    QProcess pgrep;
    pgrep.start("pgrep", {"-x", "runaway-daemon"});
    if (!pgrep.waitForFinished(1000)) {
        return false;
    }
    return pgrep.exitCode() == 0;
}

bool DaemonManager::isDaemonRunning() const
{
    // Only consider daemon running if the process is actually running
    // Socket existence alone is not reliable (could be stale)
    return checkProcessRunning();
}

void DaemonManager::tryConnect()
{
    m_reconnectTimer->stop();

    if (checkSocketExists()) {
        if (checkProcessRunning()) {
            // Socket exists AND daemon running - try to connect
            setState(State::Starting);
            m_client->connectToDaemon();
            // Result comes via onClientConnected/onClientDisconnected
        } else {
            // Stale socket (daemon not running) - remove it and start daemon
            QString socketPath = QString("/run/user/%1/runaway-guard.sock").arg(getuid());
            QFile::remove(socketPath);
            startDaemon();
        }
    } else if (checkProcessRunning()) {
        // Daemon running but socket not ready yet - poll for socket
        setState(State::Starting);
        m_socketPollTimer->start();
    } else {
        // Daemon not running - start it
        startDaemon();
    }
}

void DaemonManager::pollForSocket()
{
    static int pollCount = 0;
    pollCount++;

    if (checkSocketExists()) {
        m_socketPollTimer->stop();
        pollCount = 0;
        m_client->connectToDaemon();
    } else if (pollCount > (STARTUP_TIMEOUT_MS / SOCKET_POLL_INTERVAL_MS)) {
        // Timeout waiting for socket
        m_socketPollTimer->stop();
        pollCount = 0;
        m_lastError = tr("Timeout waiting for daemon to create socket");
        setState(State::Failed);
        emit errorOccurred(m_lastError);
    }
}

void DaemonManager::startDaemon()
{
    if (isInCrashLoop()) {
        m_lastError = tr("Daemon crash loop detected. Please check logs and restart manually.");
        setState(State::Failed);
        emit errorOccurred(m_lastError);
        return;
    }

    if (m_daemonPath.isEmpty()) {
        m_lastError = tr("Daemon binary not found");
        setState(State::Failed);
        emit errorOccurred(m_lastError);
        return;
    }

    // Clean up old process if any
    if (m_daemonProcess) {
        m_daemonProcess->disconnect();
        if (m_daemonProcess->state() != QProcess::NotRunning) {
            m_daemonProcess->terminate();
            m_daemonProcess->waitForFinished(1000);
        }
        m_daemonProcess->deleteLater();
    }

    m_daemonProcess = new QProcess(this);
    connect(m_daemonProcess, &QProcess::finished, this, &DaemonManager::onProcessFinished);
    connect(m_daemonProcess, &QProcess::errorOccurred, this, &DaemonManager::onProcessError);

    // Configure process
    m_daemonProcess->setProgram(m_daemonPath);
    m_daemonProcess->setProcessChannelMode(QProcess::ForwardedChannels);

    // Set environment for logging
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains("RUST_LOG")) {
        env.insert("RUST_LOG", "info");
    }
    m_daemonProcess->setProcessEnvironment(env);

    setState(State::Starting);
    m_daemonProcess->start();

    if (!m_daemonProcess->waitForStarted(5000)) {
        m_lastError = tr("Failed to start daemon: %1").arg(m_daemonProcess->errorString());
        setState(State::Failed);
        emit errorOccurred(m_lastError);
        return;
    }

    recordRestartAttempt();

    // Start polling for socket
    m_socketPollTimer->start();
}

void DaemonManager::stopDaemon()
{
    m_reconnectTimer->stop();
    m_socketPollTimer->stop();

    if (m_daemonProcess && m_daemonProcess->state() == QProcess::Running) {
        m_daemonProcess->terminate();
        if (!m_daemonProcess->waitForFinished(3000)) {
            m_daemonProcess->kill();
            m_daemonProcess->waitForFinished(1000);
        }
    }

    setState(State::Stopped);
    emit daemonStopped();
}

void DaemonManager::restartDaemon()
{
    stopDaemon();
    QTimer::singleShot(500, this, &DaemonManager::startDaemon);
}

void DaemonManager::onClientConnected()
{
    m_socketPollTimer->stop();
    m_reconnectAttempts = 0;
    setState(State::Running);
    emit connected();
    emit daemonStarted();
}

void DaemonManager::onClientDisconnected()
{
    emit disconnected();

    if (m_state == State::Running) {
        // Was connected, now disconnected
        if (checkProcessRunning()) {
            // Daemon still running - try reconnect
            scheduleReconnect();
        } else {
            // Daemon crashed
            setState(State::Stopped);
            emit daemonCrashed();

            // Auto-restart after brief cooldown
            if (!isInCrashLoop()) {
                QTimer::singleShot(2000, this, &DaemonManager::startDaemon);
            } else {
                m_lastError = tr("Daemon crashed repeatedly. Manual restart required.");
                setState(State::Failed);
                emit errorOccurred(m_lastError);
            }
        }
    }
}

void DaemonManager::scheduleReconnect()
{
    m_reconnectAttempts++;
    if (m_reconnectAttempts < MAX_RECONNECT_BEFORE_RESTART) {
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
    } else {
        // Too many reconnect failures - restart daemon
        m_reconnectAttempts = 0;
        restartDaemon();
    }
}

void DaemonManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    if (status == QProcess::CrashExit) {
        emit daemonCrashed();
    }
    if (m_state == State::Running || m_state == State::Starting) {
        setState(State::Stopped);
    }
}

void DaemonManager::onProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error) {
    case QProcess::FailedToStart:
        errorStr = tr("Daemon failed to start");
        break;
    case QProcess::Crashed:
        errorStr = tr("Daemon crashed");
        break;
    case QProcess::Timedout:
        errorStr = tr("Daemon operation timed out");
        break;
    default:
        errorStr = tr("Daemon process error");
        break;
    }
    m_lastError = errorStr;
    emit errorOccurred(errorStr);
}

void DaemonManager::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void DaemonManager::recordRestartAttempt()
{
    m_restartTimes.append(QDateTime::currentDateTime());
    // Keep only recent entries
    while (m_restartTimes.size() > 10) {
        m_restartTimes.removeFirst();
    }
}

bool DaemonManager::isInCrashLoop() const
{
    QDateTime now = QDateTime::currentDateTime();
    int recentRestarts = 0;
    for (const auto &time : m_restartTimes) {
        if (time.secsTo(now) < CRASH_LOOP_WINDOW_SECS) {
            recentRestarts++;
        }
    }
    return recentRestarts >= CRASH_LOOP_THRESHOLD;
}
