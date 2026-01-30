#include "ProcessTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonObject>

ProcessTab::ProcessTab(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this)) { setupUi(); }

void ProcessTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("PID"), tr("Name"), tr("CPU %"), tr("Memory (MB)"), tr("Runtime"), tr("Status")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);
}

void ProcessTab::updateProcessList(const QJsonArray &processes)
{
    m_table->setRowCount(processes.size());
    for (int i = 0; i < processes.size(); ++i) {
        QJsonObject proc = processes[i].toObject();
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(proc["pid"].toInt())));
        m_table->setItem(i, 1, new QTableWidgetItem(proc["name"].toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(proc["cpu_percent"].toDouble(), 'f', 1)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(proc["memory_mb"].toDouble(), 'f', 1)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(proc["runtime_seconds"].toInt())));
        m_table->setItem(i, 5, new QTableWidgetItem(proc["state"].toString()));
    }
}
