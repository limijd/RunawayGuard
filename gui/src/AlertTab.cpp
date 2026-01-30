#include "AlertTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonObject>

AlertTab::AlertTab(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this)) { setupUi(); }

void AlertTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("PID"), tr("Name"), tr("Reason"), tr("Severity")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);
}

void AlertTab::updateAlertList(const QJsonArray &alerts)
{
    m_table->setRowCount(alerts.size());
    for (int i = 0; i < alerts.size(); ++i) {
        QJsonObject alert = alerts[i].toObject();
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(alert["timestamp"].toInt())));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(alert["pid"].toInt())));
        m_table->setItem(i, 2, new QTableWidgetItem(alert["name"].toString()));
        m_table->setItem(i, 3, new QTableWidgetItem(alert["reason"].toString()));
        m_table->setItem(i, 4, new QTableWidgetItem(alert["severity"].toString()));
    }
}
