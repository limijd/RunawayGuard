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
{
    setupMenu();
    updateIcon();
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
    m_menu->addAction(tr("Open"), m_mainWindow, &QWidget::show);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), qApp, &QApplication::quit);
    setContextMenu(m_menu);
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
            tooltip = tr("RunawayGuard - Alerts detected");
            break;
        case Status::Critical:
            tooltip = tr("RunawayGuard - Disconnected");
            break;
    }
    setToolTip(tooltip);
}
