#include "ProcessTab.h"
#include "FormatUtils.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>

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
    layout->setContentsMargins(8, 8, 8, 8);

    // Search bar
    auto *searchLayout = new QHBoxLayout();
    searchLayout->addStretch();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search processes..."));
    m_searchEdit->setMaximumWidth(250);
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);

    // Table setup
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({tr("PID"), tr("Name"), tr("CPU"), tr("Memory"), tr("Runtime"), tr("State")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->verticalHeader()->defaultSectionSize() + 4);
    layout->addWidget(m_table);

    // Context menu
    m_contextMenu->addAction(tr("Terminate (SIGTERM)"), this, &ProcessTab::onTerminateProcess);
    m_contextMenu->addAction(tr("Kill (SIGKILL)"), this, &ProcessTab::onKillProcess);
    m_contextMenu->addAction(tr("Stop (SIGSTOP)"), this, &ProcessTab::onStopProcess);
    m_contextMenu->addAction(tr("Continue (SIGCONT)"), this, &ProcessTab::onContinueProcess);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Add to Whitelist"), this, &ProcessTab::onAddToWhitelist);

    // Connections
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &ProcessTab::showContextMenu);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ProcessTab::filterTable);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (auto *item = m_table->item(row, col)) {
            QApplication::clipboard()->setText(item->text());
        }
    });

    // Ctrl+F shortcut
    auto *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, m_searchEdit, qOverload<>(&QLineEdit::setFocus));

    // Load saved column widths
    QSettings settings;
    settings.beginGroup("ProcessTab");
    if (settings.contains("columnWidths")) {
        QList<QVariant> widths = settings.value("columnWidths").toList();
        for (int i = 0; i < qMin(widths.size(), m_table->columnCount()); ++i) {
            m_table->setColumnWidth(i, widths[i].toInt());
        }
    }
    settings.endGroup();
}

void ProcessTab::updateProcessList(const QJsonArray &processes)
{
    int sortColumn = m_table->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_table->horizontalHeader()->sortIndicatorOrder();

    m_table->setSortingEnabled(false);
    m_table->setRowCount(processes.size());

    for (int i = 0; i < processes.size(); ++i) {
        QJsonObject proc = processes[i].toObject();

        int pid = proc["pid"].toInt();
        QString name = proc["name"].toString();
        double cpu = proc["cpu_percent"].toDouble();
        double memory = proc["memory_mb"].toDouble();
        qint64 runtime = proc["runtime_seconds"].toInteger();
        QString state = proc["state"].toString();
        QString cmdline = proc["cmdline"].toString();

        auto *pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, pid);
        m_table->setItem(i, 0, pidItem);

        auto *nameItem = new QTableWidgetItem(name);
        nameItem->setToolTip(cmdline.isEmpty() ? name : cmdline);
        nameItem->setData(Qt::UserRole, cmdline);
        m_table->setItem(i, 1, nameItem);

        auto *cpuItem = new QTableWidgetItem(FormatUtils::formatCpu(cpu));
        cpuItem->setData(Qt::UserRole, cpu);
        cpuItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 2, cpuItem);

        auto *memItem = new QTableWidgetItem(FormatUtils::formatMemory(memory));
        memItem->setData(Qt::UserRole, memory);
        memItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 3, memItem);

        auto *runtimeItem = new QTableWidgetItem(FormatUtils::formatRuntime(runtime));
        runtimeItem->setData(Qt::UserRole, runtime);
        runtimeItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 4, runtimeItem);

        auto *stateItem = new QTableWidgetItem(state);
        m_table->setItem(i, 5, stateItem);

        applyRowColors(i, cpu, memory, state);
    }

    m_table->setSortingEnabled(true);
    if (sortColumn >= 0) {
        m_table->sortByColumn(sortColumn, sortOrder);
    }

    if (!m_searchEdit->text().isEmpty()) {
        filterTable(m_searchEdit->text());
    }

    QSettings settings;
    settings.beginGroup("ProcessTab");
    QList<QVariant> widths;
    for (int i = 0; i < m_table->columnCount(); ++i) {
        widths.append(m_table->columnWidth(i));
    }
    settings.setValue("columnWidths", widths);
    settings.endGroup();
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

void ProcessTab::applyRowColors(int row, double cpu, double memory, const QString &state)
{
    QColor cpuBg = FormatUtils::getCpuBackgroundColor(cpu);
    QColor memBg = FormatUtils::getMemoryBackgroundColor(memory);
    QColor stateBg = FormatUtils::getStateBackgroundColor(state);

    if (cpuBg.isValid()) {
        m_table->item(row, 2)->setBackground(cpuBg);
        m_table->item(row, 2)->setForeground(FormatUtils::getTextColorForBackground(cpuBg));
    }
    if (memBg.isValid()) {
        m_table->item(row, 3)->setBackground(memBg);
        m_table->item(row, 3)->setForeground(FormatUtils::getTextColorForBackground(memBg));
    }
    if (stateBg.isValid()) {
        m_table->item(row, 5)->setBackground(stateBg);
        m_table->item(row, 5)->setForeground(FormatUtils::getTextColorForBackground(stateBg));
    }
}

void ProcessTab::filterTable(const QString &text)
{
    for (int i = 0; i < m_table->rowCount(); ++i) {
        bool match = text.isEmpty();
        if (!match) {
            QString pid = m_table->item(i, 0)->text();
            QString name = m_table->item(i, 1)->text();
            QString cmdline = m_table->item(i, 1)->data(Qt::UserRole).toString();
            match = pid.contains(text, Qt::CaseInsensitive) ||
                    name.contains(text, Qt::CaseInsensitive) ||
                    cmdline.contains(text, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(i, !match);
    }
}

QString ProcessTab::getSelectedCmdline() const
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return QString();
    int row = items.first()->row();
    return m_table->item(row, 1)->data(Qt::UserRole).toString();
}
