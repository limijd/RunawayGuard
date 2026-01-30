#include "ProcessTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonObject>

ProcessTab::ProcessTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_contextMenu(new QMenu(this))
{
    setupUi();
}

void ProcessTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("PID"), tr("Name"), tr("CPU %"), tr("Memory (MB)"), tr("Runtime"), tr("Status")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table);

    // Setup context menu
    m_contextMenu->addAction(tr("Terminate (SIGTERM)"), this, &ProcessTab::onTerminateProcess);
    m_contextMenu->addAction(tr("Kill (SIGKILL)"), this, &ProcessTab::onKillProcess);
    m_contextMenu->addAction(tr("Stop (SIGSTOP)"), this, &ProcessTab::onStopProcess);
    m_contextMenu->addAction(tr("Continue (SIGCONT)"), this, &ProcessTab::onContinueProcess);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Add to Whitelist"), this, &ProcessTab::onAddToWhitelist);

    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ProcessTab::showContextMenu);
}

void ProcessTab::updateProcessList(const QJsonArray &processes)
{
    // Remember current sorting
    int sortColumn = m_table->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_table->horizontalHeader()->sortIndicatorOrder();

    m_table->setSortingEnabled(false);
    m_table->setRowCount(processes.size());

    for (int i = 0; i < processes.size(); ++i) {
        QJsonObject proc = processes[i].toObject();

        auto *pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, proc["pid"].toInt());
        m_table->setItem(i, 0, pidItem);

        m_table->setItem(i, 1, new QTableWidgetItem(proc["name"].toString()));

        auto *cpuItem = new QTableWidgetItem();
        cpuItem->setData(Qt::DisplayRole, proc["cpu_percent"].toDouble());
        m_table->setItem(i, 2, cpuItem);

        auto *memItem = new QTableWidgetItem();
        memItem->setData(Qt::DisplayRole, proc["memory_mb"].toDouble());
        m_table->setItem(i, 3, memItem);

        auto *runtimeItem = new QTableWidgetItem();
        runtimeItem->setData(Qt::DisplayRole, proc["runtime_seconds"].toInt());
        m_table->setItem(i, 4, runtimeItem);

        m_table->setItem(i, 5, new QTableWidgetItem(proc["state"].toString()));
    }

    m_table->setSortingEnabled(true);
    if (sortColumn >= 0) {
        m_table->sortByColumn(sortColumn, sortOrder);
    }
}

void ProcessTab::showContextMenu(const QPoint &pos)
{
    if (m_table->selectedItems().isEmpty()) return;
    m_contextMenu->exec(m_table->viewport()->mapToGlobal(pos));
}

int ProcessTab::getSelectedPid() const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return -1;
    int row = items.first()->row();
    return m_table->item(row, 0)->data(Qt::DisplayRole).toInt();
}

QString ProcessTab::getSelectedName() const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return QString();
    int row = items.first()->row();
    return m_table->item(row, 1)->text();
}

void ProcessTab::onTerminateProcess()
{
    int pid = getSelectedPid();
    if (pid > 0) emit killProcessRequested(pid, "SIGTERM");
}

void ProcessTab::onKillProcess()
{
    int pid = getSelectedPid();
    if (pid > 0) emit killProcessRequested(pid, "SIGKILL");
}

void ProcessTab::onStopProcess()
{
    int pid = getSelectedPid();
    if (pid > 0) emit killProcessRequested(pid, "SIGSTOP");
}

void ProcessTab::onContinueProcess()
{
    int pid = getSelectedPid();
    if (pid > 0) emit killProcessRequested(pid, "SIGCONT");
}

void ProcessTab::onAddToWhitelist()
{
    QString name = getSelectedName();
    if (!name.isEmpty()) emit addWhitelistRequested(name, "name");
}
