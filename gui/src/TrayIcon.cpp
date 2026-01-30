#include "TrayIcon.h"
#include "MainWindow.h"
#include <QApplication>

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
    setIcon(QIcon::fromTheme("dialog-information"));
}
