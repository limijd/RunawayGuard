#include "TrayIcon.h"
#include "MainWindow.h"
#include <QApplication>

TrayIcon::TrayIcon(MainWindow *mainWindow, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
    , m_menu(new QMenu())
    , m_status(Status::Normal)
    , m_isPaused(false)
    , m_processCount(0)
    , m_alertCount(0)
    , m_statusAction(nullptr)
    , m_pauseAction(nullptr)
    , m_clearAlertsAction(nullptr)
{
    setupMenu();
    updateIcon();
    updateTooltip();
    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

void TrayIcon::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        updateIcon();
        updateTooltip();
    }
}

void TrayIcon::updateStatusInfo(int processCount, int alertCount)
{
    m_processCount = processCount;
    m_alertCount = alertCount;
    m_clearAlertsAction->setEnabled(alertCount > 0);
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (m_mainWindow->isVisible()) {
            m_mainWindow->hide();
        } else {
            m_mainWindow->show();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();
        }
    }
}

void TrayIcon::setupMenu()
{
    // Title
    auto *titleAction = m_menu->addAction(tr("RunawayGuard"));
    titleAction->setEnabled(false);
    QFont boldFont = titleAction->font();
    boldFont.setBold(true);
    titleAction->setFont(boldFont);

    m_menu->addSeparator();

    // Status info (non-clickable)
    m_statusAction = m_menu->addAction(tr("Processes: - | Alerts: -"));
    m_statusAction->setEnabled(false);

    m_menu->addSeparator();

    // Pause/Resume
    m_pauseAction = m_menu->addAction(tr("Pause Monitoring"));
    connect(m_pauseAction, &QAction::triggered, this, &TrayIcon::onPauseToggled);

    // Clear alerts
    m_clearAlertsAction = m_menu->addAction(tr("Clear All Alerts"));
    m_clearAlertsAction->setEnabled(false);
    connect(m_clearAlertsAction, &QAction::triggered, this, &TrayIcon::clearAlertsRequested);

    m_menu->addSeparator();

    // Show/Quit
    m_menu->addAction(tr("Show Main Window"), m_mainWindow, &QWidget::show);
    m_menu->addAction(tr("Quit"), qApp, &QApplication::quit);

    setContextMenu(m_menu);

    // Update status when menu is about to show
    connect(m_menu, &QMenu::aboutToShow, this, &TrayIcon::onMenuAboutToShow);
}

void TrayIcon::onMenuAboutToShow()
{
    m_statusAction->setText(tr("Processes: %1 | Alerts: %2").arg(m_processCount).arg(m_alertCount));
}

void TrayIcon::onPauseToggled()
{
    m_isPaused = !m_isPaused;
    if (m_isPaused) {
        m_pauseAction->setText(tr("Resume Monitoring"));
        setStatus(Status::Paused);
        emit pauseRequested();
    } else {
        m_pauseAction->setText(tr("Pause Monitoring"));
        setStatus(Status::Normal);
        emit resumeRequested();
    }
}

void TrayIcon::updateIcon()
{
    QString iconPath;
    switch (m_status) {
        case Status::Normal:
            iconPath = ":/icons/tray_normal.png";
            break;
        case Status::Warning:
            iconPath = ":/icons/tray_warning.png";
            break;
        case Status::Critical:
            iconPath = ":/icons/tray_critical.png";
            break;
        case Status::Paused:
            iconPath = ":/icons/tray_paused.png";
            break;
    }
    setIcon(QIcon(iconPath));
}

void TrayIcon::updateTooltip()
{
    QString tooltip;
    switch (m_status) {
        case Status::Normal:
            tooltip = tr("RunawayGuard - All systems normal");
            break;
        case Status::Warning:
            tooltip = tr("RunawayGuard - Alerts detected (%1)").arg(m_alertCount);
            break;
        case Status::Critical:
            tooltip = tr("RunawayGuard - Disconnected");
            break;
        case Status::Paused:
            tooltip = tr("RunawayGuard - Monitoring paused");
            break;
    }
    setToolTip(tooltip);
}
