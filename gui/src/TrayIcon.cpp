#include "TrayIcon.h"
#include "MainWindow.h"
#include <QApplication>
#include <QPainter>
#include <QPixmap>

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
    // Create a colored icon based on status
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw outer circle (border)
    painter.setPen(QPen(Qt::darkGray, 2));

    // Fill color based on status
    QColor fillColor;
    switch (m_status) {
        case Status::Normal:
            fillColor = QColor(76, 175, 80);  // Green
            break;
        case Status::Warning:
            fillColor = QColor(255, 193, 7);  // Yellow/Amber
            break;
        case Status::Critical:
            fillColor = QColor(244, 67, 54);  // Red
            break;
        case Status::Paused:
            fillColor = QColor(158, 158, 158);  // Gray
            break;
    }

    painter.setBrush(fillColor);
    painter.drawEllipse(4, 4, 56, 56);

    // Draw "R" letter in center
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(36);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "R");

    painter.end();

    setIcon(QIcon(pixmap));
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
