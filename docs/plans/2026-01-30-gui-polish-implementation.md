# GUI Polish Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Comprehensively polish RunawayGuard GUI with improved data display, enhanced interactions, and visual refinements while maintaining native Qt system style.

**Architecture:** Incremental enhancement of existing Qt widgets. Add utility functions for formatting, extend DaemonClient for new protocol commands, enhance TrayIcon menu, and add QSettings persistence for user preferences.

**Tech Stack:** Qt6 (Widgets, Network), C++17, QSettings for persistence, Rust daemon protocol extensions

---

## Task 1: Add Utility Functions for Data Formatting

**Files:**
- Create: `gui/src/FormatUtils.h`
- Create: `gui/src/FormatUtils.cpp`
- Modify: `gui/CMakeLists.txt:12-22` (add to SOURCES)

**Step 1: Create FormatUtils.h**

```cpp
#ifndef FORMATUTILS_H
#define FORMATUTILS_H

#include <QString>

namespace FormatUtils {

// Format runtime seconds to human-readable string
// < 60s: "45s", < 1h: "45m 30s", < 1d: "2h 15m", >= 1d: "3d 5h"
QString formatRuntime(qint64 seconds);

// Format memory to human-readable string
// < 1GB: "512.0 MB", >= 1GB: "2.3 GB"
QString formatMemory(double megabytes);

// Format CPU percentage with 1 decimal
QString formatCpu(double percent);

// Get precise tooltip for numeric values
QString getNumericTooltip(double cpu, double memoryMb, qint64 runtimeSeconds);

}

#endif
```

**Step 2: Create FormatUtils.cpp**

```cpp
#include "FormatUtils.h"

namespace FormatUtils {

QString formatRuntime(qint64 seconds)
{
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        qint64 minutes = seconds / 60;
        qint64 secs = seconds % 60;
        return QString("%1m %2s").arg(minutes).arg(secs);
    } else if (seconds < 86400) {
        qint64 hours = seconds / 3600;
        qint64 minutes = (seconds % 3600) / 60;
        return QString("%1h %2m").arg(hours).arg(minutes);
    } else {
        qint64 days = seconds / 86400;
        qint64 hours = (seconds % 86400) / 3600;
        return QString("%1d %2h").arg(days).arg(hours);
    }
}

QString formatMemory(double megabytes)
{
    if (megabytes < 1024.0) {
        return QString("%1 MB").arg(megabytes, 0, 'f', 1);
    } else {
        return QString("%1 GB").arg(megabytes / 1024.0, 0, 'f', 1);
    }
}

QString formatCpu(double percent)
{
    return QString("%1%").arg(percent, 0, 'f', 1);
}

QString getNumericTooltip(double cpu, double memoryMb, qint64 runtimeSeconds)
{
    return QString("CPU: %1%, Memory: %2 MB, Runtime: %3 seconds")
        .arg(cpu, 0, 'f', 3)
        .arg(memoryMb, 0, 'f', 1)
        .arg(runtimeSeconds);
}

}
```

**Step 3: Update CMakeLists.txt**

Add `src/FormatUtils.cpp` to SOURCES list and `src/FormatUtils.h` to HEADERS list.

**Step 4: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds with no errors

**Step 5: Commit**

```bash
git add gui/src/FormatUtils.h gui/src/FormatUtils.cpp gui/CMakeLists.txt
git commit -m "feat(gui): Add FormatUtils for data formatting"
```

---

## Task 2: Add Threshold Coloring Utility

**Files:**
- Modify: `gui/src/FormatUtils.h` (add color functions)
- Modify: `gui/src/FormatUtils.cpp` (implement color logic)

**Step 1: Add color functions to FormatUtils.h**

Add after existing declarations:
```cpp
#include <QColor>

// Get background color for CPU value
// < 80%: transparent, 80-90%: light orange, > 90%: light red
QColor getCpuBackgroundColor(double percent);

// Get background color for memory value
// < 1GB: transparent, 1-4GB: light orange, > 4GB: light red
QColor getMemoryBackgroundColor(double megabytes);

// Get background color for process state
// R/S: transparent, D/Z: light red
QColor getStateBackgroundColor(const QString &state);

// Get text color (darker version for contrast)
QColor getTextColorForBackground(const QColor &bg);
```

**Step 2: Implement color functions in FormatUtils.cpp**

```cpp
QColor getCpuBackgroundColor(double percent)
{
    if (percent > 90.0) {
        return QColor(255, 200, 200);  // Light red
    } else if (percent > 80.0) {
        return QColor(255, 230, 200);  // Light orange
    }
    return QColor();  // Transparent/default
}

QColor getMemoryBackgroundColor(double megabytes)
{
    if (megabytes > 4096.0) {
        return QColor(255, 200, 200);  // Light red
    } else if (megabytes > 1024.0) {
        return QColor(255, 230, 200);  // Light orange
    }
    return QColor();
}

QColor getStateBackgroundColor(const QString &state)
{
    if (state == "D" || state == "Z") {
        return QColor(255, 200, 200);  // Light red
    }
    return QColor();
}

QColor getTextColorForBackground(const QColor &bg)
{
    if (!bg.isValid() || bg.alpha() == 0) {
        return QColor();  // Default text color
    }
    // Darker version of background for text
    return bg.darker(200);
}
```

**Step 3: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add gui/src/FormatUtils.h gui/src/FormatUtils.cpp
git commit -m "feat(gui): Add threshold coloring utilities"
```

---

## Task 3: Enhance ProcessTab with Formatting and Coloring

**Files:**
- Modify: `gui/src/ProcessTab.cpp`
- Modify: `gui/src/ProcessTab.h`

**Step 1: Add includes and member variables to ProcessTab.h**

Add after existing includes:
```cpp
#include <QLineEdit>
#include <QSettings>
```

Add private members:
```cpp
QLineEdit *m_searchEdit;
void applyRowColors(int row, double cpu, double memory, const QString &state);
void filterTable(const QString &text);
QString getSelectedCmdline() const;
```

**Step 2: Update ProcessTab.cpp setupUi()**

Replace the existing setupUi with enhanced version:

```cpp
#include "FormatUtils.h"
#include <QApplication>
#include <QClipboard>
#include <QShortcut>

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
            // Status bar message will be handled by MainWindow
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
```

**Step 3: Update updateProcessList() with formatting and colors**

```cpp
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

        // PID
        auto *pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, pid);
        m_table->setItem(i, 0, pidItem);

        // Name with cmdline tooltip
        auto *nameItem = new QTableWidgetItem(name);
        nameItem->setToolTip(cmdline.isEmpty() ? name : cmdline);
        nameItem->setData(Qt::UserRole, cmdline);  // Store cmdline for later use
        m_table->setItem(i, 1, nameItem);

        // CPU (formatted)
        auto *cpuItem = new QTableWidgetItem(FormatUtils::formatCpu(cpu));
        cpuItem->setData(Qt::UserRole, cpu);  // Store raw value for sorting
        cpuItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 2, cpuItem);

        // Memory (formatted)
        auto *memItem = new QTableWidgetItem(FormatUtils::formatMemory(memory));
        memItem->setData(Qt::UserRole, memory);
        memItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 3, memItem);

        // Runtime (formatted)
        auto *runtimeItem = new QTableWidgetItem(FormatUtils::formatRuntime(runtime));
        runtimeItem->setData(Qt::UserRole, runtime);
        runtimeItem->setToolTip(FormatUtils::getNumericTooltip(cpu, memory, runtime));
        m_table->setItem(i, 4, runtimeItem);

        // State
        auto *stateItem = new QTableWidgetItem(state);
        m_table->setItem(i, 5, stateItem);

        // Apply colors
        applyRowColors(i, cpu, memory, state);
    }

    m_table->setSortingEnabled(true);
    if (sortColumn >= 0) {
        m_table->sortByColumn(sortColumn, sortOrder);
    }

    // Re-apply filter if active
    if (!m_searchEdit->text().isEmpty()) {
        filterTable(m_searchEdit->text());
    }

    // Save column widths
    QSettings settings;
    settings.beginGroup("ProcessTab");
    QList<QVariant> widths;
    for (int i = 0; i < m_table->columnCount(); ++i) {
        widths.append(m_table->columnWidth(i));
    }
    settings.setValue("columnWidths", widths);
    settings.endGroup();
}
```

**Step 4: Add helper methods**

```cpp
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
            // Check PID, Name, and cmdline (stored in UserRole)
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
```

**Step 5: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 6: Commit**

```bash
git add gui/src/ProcessTab.h gui/src/ProcessTab.cpp
git commit -m "feat(gui): Enhance ProcessTab with formatting, coloring, and search"
```

---

## Task 4: Enhance AlertTab with Same Improvements

**Files:**
- Modify: `gui/src/AlertTab.h`
- Modify: `gui/src/AlertTab.cpp`

**Step 1: Update AlertTab.h**

Add includes and members similar to ProcessTab:
```cpp
#include <QLineEdit>

// Add private members:
QLineEdit *m_searchEdit;
void filterTable(const QString &text);
```

**Step 2: Update AlertTab.cpp setupUi()**

```cpp
#include "FormatUtils.h"
#include <QApplication>
#include <QClipboard>
#include <QShortcut>

void AlertTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    // Search bar
    auto *searchLayout = new QHBoxLayout();
    searchLayout->addStretch();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search alerts..."));
    m_searchEdit->setMaximumWidth(250);
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);

    // Table
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Time"), tr("PID"), tr("Name"), tr("Reason"), tr("Severity")});
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
    m_contextMenu->addAction(tr("Add to Whitelist"), this, &AlertTab::onAddToWhitelist);
    m_contextMenu->addAction(tr("Terminate Process"), this, &AlertTab::onTerminateProcess);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(tr("Dismiss Alert"), this, &AlertTab::onDismissAlert);

    // Connections
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &AlertTab::showContextMenu);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AlertTab::filterTable);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (auto *item = m_table->item(row, col)) {
            QApplication::clipboard()->setText(item->text());
        }
    });

    // Ctrl+F shortcut
    auto *searchShortcut = new QShortcut(QKeySequence::Find, this);
    connect(searchShortcut, &QShortcut::activated, m_searchEdit, qOverload<>(&QLineEdit::setFocus));
}
```

**Step 3: Update updateAlertList() with severity coloring**

```cpp
void AlertTab::updateAlertList(const QJsonArray &alerts)
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(alerts.size());

    for (int i = 0; i < alerts.size(); ++i) {
        QJsonObject alert = alerts[i].toObject();

        qint64 timestamp = alert["timestamp"].toInteger();
        QString timeStr = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss");
        int pid = alert["pid"].toInt();
        QString name = alert["name"].toString();
        QString reason = alert["reason"].toString();
        QString severity = alert["severity"].toString();

        m_table->setItem(i, 0, new QTableWidgetItem(timeStr));

        auto *pidItem = new QTableWidgetItem();
        pidItem->setData(Qt::DisplayRole, pid);
        m_table->setItem(i, 1, pidItem);

        m_table->setItem(i, 2, new QTableWidgetItem(name));
        m_table->setItem(i, 3, new QTableWidgetItem(reason));

        auto *severityItem = new QTableWidgetItem(severity);
        if (severity == "critical") {
            severityItem->setBackground(QColor(255, 200, 200));
            severityItem->setForeground(QColor(150, 0, 0));
        } else if (severity == "warning") {
            severityItem->setBackground(QColor(255, 230, 200));
            severityItem->setForeground(QColor(150, 100, 0));
        }
        m_table->setItem(i, 4, severityItem);
    }

    m_table->setSortingEnabled(true);

    if (!m_searchEdit->text().isEmpty()) {
        filterTable(m_searchEdit->text());
    }
}

void AlertTab::filterTable(const QString &text)
{
    for (int i = 0; i < m_table->rowCount(); ++i) {
        bool match = text.isEmpty();
        if (!match) {
            for (int col = 0; col < m_table->columnCount(); ++col) {
                if (m_table->item(i, col)->text().contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        m_table->setRowHidden(i, !match);
    }
}
```

**Step 4: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add gui/src/AlertTab.h gui/src/AlertTab.cpp
git commit -m "feat(gui): Enhance AlertTab with search, coloring, and improved layout"
```

---

## Task 5: Enhance WhitelistTab Layout

**Files:**
- Modify: `gui/src/WhitelistTab.cpp`

**Step 1: Update setupUi() with improved layout**

```cpp
void WhitelistTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Input area with better styling
    auto *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);
    inputLayout->addWidget(new QLabel(tr("Pattern:"), this));
    m_patternEdit->setPlaceholderText(tr("Process name or pattern"));
    inputLayout->addWidget(m_patternEdit, 1);

    inputLayout->addWidget(new QLabel(tr("Match:"), this));
    m_matchTypeCombo->addItem(tr("Name"), "name");
    m_matchTypeCombo->addItem(tr("Command"), "cmdline");
    m_matchTypeCombo->addItem(tr("Regex"), "regex");
    inputLayout->addWidget(m_matchTypeCombo);

    inputLayout->addWidget(m_addButton);
    inputLayout->addWidget(m_removeButton);

    layout->addLayout(inputLayout);

    // Table with improved settings
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Pattern"), tr("Match Type"), tr("Reason")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setColumnWidth(0, 200);
    m_table->setColumnWidth(1, 100);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->verticalHeader()->defaultSectionSize() + 4);
    layout->addWidget(m_table);

    // Connections
    connect(m_addButton, &QPushButton::clicked, this, &WhitelistTab::onAddClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &WhitelistTab::onRemoveClicked);
    connect(m_patternEdit, &QLineEdit::returnPressed, this, &WhitelistTab::onAddClicked);
}
```

**Step 2: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add gui/src/WhitelistTab.cpp
git commit -m "feat(gui): Improve WhitelistTab layout with better spacing"
```

---

## Task 6: Enhance TrayIcon with Menu and Pause Feature

**Files:**
- Modify: `gui/src/TrayIcon.h`
- Modify: `gui/src/TrayIcon.cpp`

**Step 1: Update TrayIcon.h**

```cpp
#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QWidgetAction>

class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(MainWindow *mainWindow, QObject *parent = nullptr);

    enum class Status { Normal, Warning, Critical, Paused };
    void setStatus(Status status);
    void updateStatusInfo(int processCount, int alertCount);

signals:
    void pauseRequested();
    void resumeRequested();
    void clearAlertsRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void onMenuAboutToShow();
    void onPauseToggled();

private:
    void setupMenu();
    void updateIcon();
    void updateTooltip();

    MainWindow *m_mainWindow;
    QMenu *m_menu;
    Status m_status;
    bool m_isPaused;
    int m_processCount;
    int m_alertCount;

    QAction *m_statusAction;
    QAction *m_pauseAction;
    QAction *m_clearAlertsAction;
};

#endif
```

**Step 2: Update TrayIcon.cpp**

```cpp
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
```

**Step 3: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds (note: tray_paused.png not yet created, will add in resources task)

**Step 4: Commit**

```bash
git add gui/src/TrayIcon.h gui/src/TrayIcon.cpp
git commit -m "feat(gui): Enhance TrayIcon with status info and pause control"
```

---

## Task 7: Update MainWindow for New Features

**Files:**
- Modify: `gui/src/MainWindow.h`
- Modify: `gui/src/MainWindow.cpp`

**Step 1: Update MainWindow.h**

Add new slots and signals:
```cpp
// Add to private slots:
void showStatusMessage(const QString &message, int timeout = 3000);
void onCellDoubleClicked(const QString &text);
void onPauseMonitoring();
void onResumeMonitoring();
void onClearAlerts();

// Add private members:
void saveWindowState();
void restoreWindowState();
```

**Step 2: Update MainWindow.cpp**

Add window state persistence and status messages:

```cpp
#include <QSettings>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    // ... existing initialization ...
{
    setupUi();
    setupStatusBar();
    setupConnections();
    setupTrayIcon();
    restoreWindowState();

    m_statusLabel->setText(tr("Starting daemon..."));
    m_daemonManager->initialize();

    m_refreshTimer->setInterval(10000);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshData);
}

MainWindow::~MainWindow()
{
    saveWindowState();
}

void MainWindow::setupUi()
{
    setWindowTitle("RunawayGuard");
    setWindowIcon(QIcon(":/icons/app_icon.png"));
    setMinimumSize(600, 400);
    resize(800, 600);

    // Add tab icons
    m_tabWidget->addTab(m_processTab, QIcon(":/icons/tab_monitor.png"), tr("Monitor"));
    m_tabWidget->addTab(m_alertTab, QIcon(":/icons/tab_alerts.png"), tr("Alerts"));
    m_tabWidget->addTab(m_whitelistTab, QIcon(":/icons/tab_whitelist.png"), tr("Whitelist"));
    m_tabWidget->addTab(m_settingsTab, QIcon(":/icons/tab_settings.png"), tr("Settings"));

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupConnections()
{
    // ... existing connections ...

    // TrayIcon connections
    connect(m_trayIcon, &TrayIcon::pauseRequested, this, &MainWindow::onPauseMonitoring);
    connect(m_trayIcon, &TrayIcon::resumeRequested, this, &MainWindow::onResumeMonitoring);
    connect(m_trayIcon, &TrayIcon::clearAlertsRequested, this, &MainWindow::onClearAlerts);
}

void MainWindow::onStatusReceived(const QJsonObject &status)
{
    int processCount = status["monitored_count"].toInt();
    int alertCount = status["alert_count"].toInt();

    m_processCountLabel->setText(tr("Processes: %1").arg(processCount));
    m_alertCountLabel->setText(tr("Alerts: %1").arg(alertCount));

    // Update tray icon
    m_trayIcon->updateStatusInfo(processCount, alertCount);
    if (alertCount > 0) {
        m_trayIcon->setStatus(TrayIcon::Status::Warning);
    } else {
        m_trayIcon->setStatus(TrayIcon::Status::Normal);
    }
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void MainWindow::onPauseMonitoring()
{
    // TODO: Send pause command to daemon when implemented
    showStatusMessage(tr("Monitoring paused"));
}

void MainWindow::onResumeMonitoring()
{
    // TODO: Send resume command to daemon when implemented
    showStatusMessage(tr("Monitoring resumed"));
}

void MainWindow::onClearAlerts()
{
    // TODO: Send clear alerts command to daemon when implemented
    showStatusMessage(tr("All alerts cleared"));
}

void MainWindow::saveWindowState()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();
}

void MainWindow::restoreWindowState()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
    settings.endGroup();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowState();
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}
```

**Step 3: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add gui/src/MainWindow.h gui/src/MainWindow.cpp
git commit -m "feat(gui): Add window state persistence and status messages"
```

---

## Task 8: Add SIGKILL Confirmation Dialog

**Files:**
- Modify: `gui/src/ProcessTab.cpp`

**Step 1: Update onKillProcess() with confirmation**

```cpp
#include <QMessageBox>

void ProcessTab::onKillProcess()
{
    int pid = getSelectedPid();
    QString name = getSelectedName();
    if (pid <= 0) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Force Kill"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(tr("Are you sure you want to force kill this process?"));
    msgBox.setInformativeText(tr("Process: %1 (PID: %2)\n\nForce killing may cause data loss.").arg(name).arg(pid));
    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.button(QMessageBox::Yes)->setText(tr("Force Kill"));

    if (msgBox.exec() == QMessageBox::Yes) {
        emit killProcessRequested(pid, "SIGKILL");
    }
}
```

**Step 2: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add gui/src/ProcessTab.cpp
git commit -m "feat(gui): Add confirmation dialog for SIGKILL"
```

---

## Task 9: Extend Daemon Protocol for Config Sync

**Files:**
- Modify: `daemon/src/protocol.rs`

**Step 1: Add GetConfig request and ConfigResponse**

```rust
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "cmd", rename_all = "snake_case")]
pub enum Request {
    Ping,
    ListProcesses,
    GetAlerts { params: GetAlertsParams },
    KillProcess { params: KillProcessParams },
    ListWhitelist,
    AddWhitelist { params: AddWhitelistParams },
    RemoveWhitelist { params: RemoveWhitelistParams },
    UpdateConfig { params: serde_json::Value },
    GetConfig,  // NEW
    PauseMonitoring,  // NEW
    ResumeMonitoring,  // NEW
    ClearAlerts,  // NEW
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Response {
    Pong,
    Response { id: Option<String>, data: serde_json::Value },
    Alert { data: AlertData },
    Status { data: StatusData },
    Config { data: ConfigData },  // NEW
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConfigData {
    pub cpu_high: CpuHighConfig,
    pub hang: HangConfig,
    pub memory_leak: MemoryLeakConfig,
    pub general: GeneralConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CpuHighConfig {
    pub enabled: bool,
    pub threshold_percent: u32,
    pub duration_seconds: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HangConfig {
    pub enabled: bool,
    pub duration_seconds: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryLeakConfig {
    pub enabled: bool,
    pub growth_threshold_mb: u32,
    pub window_minutes: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    pub sample_interval_normal: u32,
    pub sample_interval_alert: u32,
    pub notification_method: String,
}
```

**Step 2: Build daemon**

Run: `cd daemon && cargo-1.89 build`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add daemon/src/protocol.rs
git commit -m "feat(daemon): Extend protocol with config and control commands"
```

---

## Task 10: Add DaemonClient Methods for New Commands

**Files:**
- Modify: `gui/src/DaemonClient.h`
- Modify: `gui/src/DaemonClient.cpp`

**Step 1: Add new signals and methods to DaemonClient.h**

```cpp
// Add signals:
void configReceived(const QJsonObject &config);

// Add methods:
void requestConfig();
void requestUpdateConfig(const QJsonObject &config);
void requestPauseMonitoring();
void requestResumeMonitoring();
void requestClearAlerts();
```

**Step 2: Implement new methods in DaemonClient.cpp**

```cpp
void DaemonClient::requestConfig()
{
    QJsonObject request;
    request["cmd"] = "get_config";
    sendRequest(request);
}

void DaemonClient::requestUpdateConfig(const QJsonObject &config)
{
    QJsonObject request;
    request["cmd"] = "update_config";
    request["params"] = config;
    sendRequest(request);
}

void DaemonClient::requestPauseMonitoring()
{
    QJsonObject request;
    request["cmd"] = "pause_monitoring";
    sendRequest(request);
}

void DaemonClient::requestResumeMonitoring()
{
    QJsonObject request;
    request["cmd"] = "resume_monitoring";
    sendRequest(request);
}

void DaemonClient::requestClearAlerts()
{
    QJsonObject request;
    request["cmd"] = "clear_alerts";
    sendRequest(request);
}
```

**Step 3: Update onReadyRead() to handle config response**

Add in the response parsing section:
```cpp
if (type == "config") {
    emit configReceived(doc.object()["data"].toObject());
}
```

**Step 4: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 5: Commit**

```bash
git add gui/src/DaemonClient.h gui/src/DaemonClient.cpp
git commit -m "feat(gui): Add DaemonClient methods for config and control"
```

---

## Task 11: Complete SettingsTab with Config Sync

**Files:**
- Modify: `gui/src/SettingsTab.h`
- Modify: `gui/src/SettingsTab.cpp`

**Step 1: Update SettingsTab.h**

```cpp
#ifndef SETTINGSTAB_H
#define SETTINGSTAB_H

#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QJsonObject>

class SettingsTab : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsTab(QWidget *parent = nullptr);

signals:
    void configUpdateRequested(const QJsonObject &config);

public slots:
    void loadConfig(const QJsonObject &config);
    void setConnected(bool connected);

private slots:
    void onApplyClicked();
    void onResetClicked();
    void onSettingChanged();

private:
    void setupUi();
    void setModified(bool modified);
    QJsonObject collectConfig() const;

    // ... existing members ...

    bool m_isModified;
    bool m_isConnected;
    QJsonObject m_originalConfig;
};

#endif
```

**Step 2: Update SettingsTab.cpp**

```cpp
#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
    , m_cpuEnabled(new QCheckBox(tr("Enable"), this))
    // ... other initializations ...
    , m_isModified(false)
    , m_isConnected(false)
{
    setupUi();
}

void SettingsTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // CPU Detection Group
    auto *cpuGroup = new QGroupBox(tr("CPU High Detection"), this);
    auto *cpuLayout = new QFormLayout(cpuGroup);
    cpuLayout->setContentsMargins(12, 12, 12, 12);
    cpuLayout->addRow(m_cpuEnabled);
    m_cpuThreshold->setRange(1, 100);
    m_cpuThreshold->setSuffix("%");
    m_cpuThreshold->setFixedWidth(80);
    cpuLayout->addRow(tr("Threshold:"), m_cpuThreshold);
    m_cpuDuration->setRange(1, 3600);
    m_cpuDuration->setSuffix(tr(" sec"));
    m_cpuDuration->setFixedWidth(80);
    cpuLayout->addRow(tr("Duration:"), m_cpuDuration);
    mainLayout->addWidget(cpuGroup);

    // ... similar for other groups with setContentsMargins(12, 12, 12, 12) ...

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_applyButton->setEnabled(false);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_resetButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();

    // Connect all settings changes to onSettingChanged
    connect(m_cpuEnabled, &QCheckBox::toggled, this, &SettingsTab::onSettingChanged);
    connect(m_cpuThreshold, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    connect(m_cpuDuration, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    // ... connect all other widgets ...

    connect(m_applyButton, &QPushButton::clicked, this, &SettingsTab::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsTab::onResetClicked);
}

void SettingsTab::loadConfig(const QJsonObject &config)
{
    m_originalConfig = config;

    // Block signals while loading
    QSignalBlocker b1(m_cpuEnabled), b2(m_cpuThreshold), b3(m_cpuDuration);
    // ... block other widgets ...

    QJsonObject cpuHigh = config["cpu_high"].toObject();
    m_cpuEnabled->setChecked(cpuHigh["enabled"].toBool());
    m_cpuThreshold->setValue(cpuHigh["threshold_percent"].toInt());
    m_cpuDuration->setValue(cpuHigh["duration_seconds"].toInt());

    QJsonObject hang = config["hang"].toObject();
    m_hangEnabled->setChecked(hang["enabled"].toBool());
    m_hangDuration->setValue(hang["duration_seconds"].toInt());

    QJsonObject memoryLeak = config["memory_leak"].toObject();
    m_memoryEnabled->setChecked(memoryLeak["enabled"].toBool());
    m_memoryGrowth->setValue(memoryLeak["growth_threshold_mb"].toInt());
    m_memoryWindow->setValue(memoryLeak["window_minutes"].toInt());

    QJsonObject general = config["general"].toObject();
    m_normalInterval->setValue(general["sample_interval_normal"].toInt());
    m_alertInterval->setValue(general["sample_interval_alert"].toInt());

    QString method = general["notification_method"].toString();
    int idx = m_notificationMethod->findData(method);
    if (idx >= 0) m_notificationMethod->setCurrentIndex(idx);

    setModified(false);
}

void SettingsTab::setConnected(bool connected)
{
    m_isConnected = connected;
    setEnabled(connected);
}

void SettingsTab::onSettingChanged()
{
    setModified(true);
}

void SettingsTab::setModified(bool modified)
{
    m_isModified = modified;
    m_applyButton->setEnabled(modified && m_isConnected);
}

void SettingsTab::onApplyClicked()
{
    emit configUpdateRequested(collectConfig());
    setModified(false);
}

void SettingsTab::onResetClicked()
{
    loadConfig(m_originalConfig);
}

QJsonObject SettingsTab::collectConfig() const
{
    QJsonObject config;

    QJsonObject cpuHigh;
    cpuHigh["enabled"] = m_cpuEnabled->isChecked();
    cpuHigh["threshold_percent"] = m_cpuThreshold->value();
    cpuHigh["duration_seconds"] = m_cpuDuration->value();
    config["cpu_high"] = cpuHigh;

    QJsonObject hang;
    hang["enabled"] = m_hangEnabled->isChecked();
    hang["duration_seconds"] = m_hangDuration->value();
    config["hang"] = hang;

    QJsonObject memoryLeak;
    memoryLeak["enabled"] = m_memoryEnabled->isChecked();
    memoryLeak["growth_threshold_mb"] = m_memoryGrowth->value();
    memoryLeak["window_minutes"] = m_memoryWindow->value();
    config["memory_leak"] = memoryLeak;

    QJsonObject general;
    general["sample_interval_normal"] = m_normalInterval->value();
    general["sample_interval_alert"] = m_alertInterval->value();
    general["notification_method"] = m_notificationMethod->currentData().toString();
    config["general"] = general;

    return config;
}
```

**Step 3: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add gui/src/SettingsTab.h gui/src/SettingsTab.cpp
git commit -m "feat(gui): Complete SettingsTab with config sync support"
```

---

## Task 12: Wire Up SettingsTab in MainWindow

**Files:**
- Modify: `gui/src/MainWindow.cpp`

**Step 1: Add connections for SettingsTab**

In setupConnections():
```cpp
// SettingsTab connections
connect(daemonClient, &DaemonClient::configReceived, m_settingsTab, &SettingsTab::loadConfig);
connect(m_settingsTab, &SettingsTab::configUpdateRequested, daemonClient, &DaemonClient::requestUpdateConfig);
```

In onConnected():
```cpp
m_settingsTab->setConnected(true);
m_daemonManager->client()->requestConfig();  // Load config on connect
```

In onDisconnected():
```cpp
m_settingsTab->setConnected(false);
```

**Step 2: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add gui/src/MainWindow.cpp
git commit -m "feat(gui): Wire up SettingsTab config sync in MainWindow"
```

---

## Task 13: Create Resource Files (Icons)

**Files:**
- Create: `gui/resources/resources.qrc`
- Create: `gui/resources/tray_paused.png` (placeholder)
- Create: `gui/resources/tab_monitor.png` (placeholder)
- Create: `gui/resources/tab_alerts.png` (placeholder)
- Create: `gui/resources/tab_whitelist.png` (placeholder)
- Create: `gui/resources/tab_settings.png` (placeholder)

**Step 1: Copy existing resources from main worktree**

```bash
cp -r /home/wli/.local/share/Cryptomator/mnt/my25-projects/RunawayGuard/gui/resources /home/wli/.local/share/Cryptomator/mnt/my25-projects/RunawayGuard/.worktrees/gui-polish/gui/
```

**Step 2: Create tray_paused.png**

Use ImageMagick or similar to create a gray version of tray_normal.png:
```bash
convert gui/resources/tray_normal.png -colorspace Gray gui/resources/tray_paused.png
```

Or create a placeholder:
```bash
# If convert not available, copy and note for later
cp gui/resources/tray_normal.png gui/resources/tray_paused.png
```

**Step 3: Create tab icons (16x16 simple icons)**

For now, create placeholder files. These can be replaced with proper icons later:
```bash
# Create simple colored placeholder icons using ImageMagick
convert -size 16x16 xc:#4CAF50 gui/resources/tab_monitor.png
convert -size 16x16 xc:#FF9800 gui/resources/tab_alerts.png
convert -size 16x16 xc:#2196F3 gui/resources/tab_whitelist.png
convert -size 16x16 xc:#9E9E9E gui/resources/tab_settings.png
```

**Step 4: Update resources.qrc**

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/icons">
        <file>app_icon.png</file>
        <file>icon_16.png</file>
        <file>icon_32.png</file>
        <file>icon_48.png</file>
        <file>icon_64.png</file>
        <file>icon_128.png</file>
        <file>icon_256.png</file>
        <file>tray_normal.png</file>
        <file>tray_warning.png</file>
        <file>tray_critical.png</file>
        <file>tray_paused.png</file>
        <file>tab_monitor.png</file>
        <file>tab_alerts.png</file>
        <file>tab_whitelist.png</file>
        <file>tab_settings.png</file>
    </qresource>
</RCC>
```

**Step 5: Build and verify**

Run: `cd gui && cmake --build build`
Expected: Build succeeds

**Step 6: Commit**

```bash
git add gui/resources/
git commit -m "feat(gui): Add icon resources for tray and tabs"
```

---

## Task 14: Final Integration Test

**Step 1: Clean build**

```bash
cd gui && rm -rf build && cmake -B build -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.2/gcc_64 && cmake --build build
```

**Step 2: Build daemon**

```bash
cd daemon && cargo-1.89 build --release
```

**Step 3: Manual testing checklist**

- [ ] Launch GUI, verify window opens with correct size
- [ ] Verify tab icons display
- [ ] Verify tray icon shows with menu
- [ ] Verify search box works in ProcessTab
- [ ] Verify CPU/Memory coloring for high values
- [ ] Verify Runtime formats correctly
- [ ] Verify double-click copies cell content
- [ ] Verify SIGKILL shows confirmation dialog
- [ ] Close and reopen, verify window position restored

**Step 4: Commit final state**

```bash
git add -A
git commit -m "feat(gui): Complete GUI polish implementation"
```

---

## Summary

| Task | Description | Files Modified |
|------|-------------|----------------|
| 1 | FormatUtils creation | +FormatUtils.h/cpp, CMakeLists.txt |
| 2 | Threshold coloring | FormatUtils.h/cpp |
| 3 | ProcessTab enhancement | ProcessTab.h/cpp |
| 4 | AlertTab enhancement | AlertTab.h/cpp |
| 5 | WhitelistTab layout | WhitelistTab.cpp |
| 6 | TrayIcon enhancement | TrayIcon.h/cpp |
| 7 | MainWindow updates | MainWindow.h/cpp |
| 8 | SIGKILL confirmation | ProcessTab.cpp |
| 9 | Daemon protocol extension | protocol.rs |
| 10 | DaemonClient new methods | DaemonClient.h/cpp |
| 11 | SettingsTab completion | SettingsTab.h/cpp |
| 12 | SettingsTab wiring | MainWindow.cpp |
| 13 | Resource files | resources/ |
| 14 | Integration test | - |
