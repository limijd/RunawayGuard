#include "AlertTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QDateTime>

AlertTab::AlertTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_contextMenu(new QMenu(this))
{
    setupUi();
}

void AlertTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("PID"), tr("Name"), tr("Reason"), tr("Severity")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table);

    // Setup context menu
    m_contextMenu->addAction(tr("Add to Whitelist"), this, &AlertTab::onAddToWhitelist);
    m_contextMenu->addAction(tr("Terminate Process"), this, &AlertTab::onTerminateProcess);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Dismiss Alert"), this, &AlertTab::onDismissAlert);

    connect(m_table, &QTableWidget::customContextMenuRequested, this, &AlertTab::showContextMenu);
}

void AlertTab::updateAlertList(const QJsonArray &alerts)
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(alerts.size());
    for (int i = 0; i < alerts.size(); ++i) {
        QJsonObject alert = alerts[i].toObject();

        // Format timestamp as readable date/time
        qint64 timestamp = alert["timestamp"].toInteger();
        QString timeStr = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");

        m_table->setItem(i, 0, new QTableWidgetItem(timeStr));

        auto *pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, alert["pid"].toInt());
        m_table->setItem(i, 1, pidItem);

        m_table->setItem(i, 2, new QTableWidgetItem(alert["name"].toString()));
        m_table->setItem(i, 3, new QTableWidgetItem(alert["reason"].toString()));
        m_table->setItem(i, 4, new QTableWidgetItem(alert["severity"].toString()));
    }
    m_table->setSortingEnabled(true);
}

void AlertTab::showContextMenu(const QPoint &pos)
{
    if (m_table->selectedItems().isEmpty()) return;
    m_contextMenu->exec(m_table->viewport()->mapToGlobal(pos));
}

int AlertTab::getSelectedPid() const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return -1;
    int row = items.first()->row();
    return m_table->item(row, 1)->data(Qt::DisplayRole).toInt();
}

QString AlertTab::getSelectedName() const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return QString();
    int row = items.first()->row();
    return m_table->item(row, 2)->text();
}

void AlertTab::onAddToWhitelist()
{
    QString name = getSelectedName();
    if (!name.isEmpty()) {
        emit addWhitelistRequested(name, "name");
    }
}

void AlertTab::onTerminateProcess()
{
    int pid = getSelectedPid();
    if (pid > 0) {
        emit killProcessRequested(pid, "SIGTERM");
    }
}

void AlertTab::onDismissAlert()
{
    // Remove the selected row from the table (local only, doesn't affect daemon)
    auto items = m_table->selectedItems();
    if (!items.isEmpty()) {
        int row = items.first()->row();
        m_table->removeRow(row);
    }
}
