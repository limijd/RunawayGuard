#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
    , m_cpuEnabled(new QCheckBox(tr("Enable"), this))
    , m_cpuThreshold(new QSpinBox(this))
    , m_cpuDuration(new QSpinBox(this))
    , m_hangEnabled(new QCheckBox(tr("Enable"), this))
    , m_hangDuration(new QSpinBox(this))
    , m_memoryEnabled(new QCheckBox(tr("Enable"), this))
    , m_memoryGrowth(new QSpinBox(this))
    , m_memoryWindow(new QSpinBox(this))
    , m_normalInterval(new QSpinBox(this))
    , m_alertInterval(new QSpinBox(this))
    , m_notificationMethod(new QComboBox(this))
    , m_applyButton(new QPushButton(tr("Apply"), this))
    , m_resetButton(new QPushButton(tr("Reset"), this))
{
    setupUi();
    loadSettings();
}

void SettingsTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // CPU Detection Group
    auto *cpuGroup = new QGroupBox(tr("CPU High Detection"), this);
    auto *cpuLayout = new QFormLayout(cpuGroup);
    cpuLayout->addRow(m_cpuEnabled);
    m_cpuThreshold->setRange(1, 100);
    m_cpuThreshold->setSuffix("%");
    cpuLayout->addRow(tr("Threshold:"), m_cpuThreshold);
    m_cpuDuration->setRange(1, 3600);
    m_cpuDuration->setSuffix(tr(" sec"));
    cpuLayout->addRow(tr("Duration:"), m_cpuDuration);
    mainLayout->addWidget(cpuGroup);

    // Hang Detection Group
    auto *hangGroup = new QGroupBox(tr("Hang Detection"), this);
    auto *hangLayout = new QFormLayout(hangGroup);
    hangLayout->addRow(m_hangEnabled);
    m_hangDuration->setRange(1, 3600);
    m_hangDuration->setSuffix(tr(" sec"));
    hangLayout->addRow(tr("Duration:"), m_hangDuration);
    mainLayout->addWidget(hangGroup);

    // Memory Detection Group
    auto *memoryGroup = new QGroupBox(tr("Memory Leak Detection"), this);
    auto *memoryLayout = new QFormLayout(memoryGroup);
    memoryLayout->addRow(m_memoryEnabled);
    m_memoryGrowth->setRange(1, 10000);
    m_memoryGrowth->setSuffix(tr(" MB"));
    memoryLayout->addRow(tr("Growth threshold:"), m_memoryGrowth);
    m_memoryWindow->setRange(1, 60);
    m_memoryWindow->setSuffix(tr(" min"));
    memoryLayout->addRow(tr("Time window:"), m_memoryWindow);
    mainLayout->addWidget(memoryGroup);

    // General Settings Group
    auto *generalGroup = new QGroupBox(tr("General"), this);
    auto *generalLayout = new QFormLayout(generalGroup);
    m_normalInterval->setRange(1, 300);
    m_normalInterval->setSuffix(tr(" sec"));
    generalLayout->addRow(tr("Normal interval:"), m_normalInterval);
    m_alertInterval->setRange(1, 60);
    m_alertInterval->setSuffix(tr(" sec"));
    generalLayout->addRow(tr("Alert interval:"), m_alertInterval);
    m_notificationMethod->addItem(tr("System + Popup"), "both");
    m_notificationMethod->addItem(tr("System only"), "system");
    m_notificationMethod->addItem(tr("Popup only"), "popup");
    generalLayout->addRow(tr("Notification:"), m_notificationMethod);
    mainLayout->addWidget(generalGroup);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_resetButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();

    // Connections
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsTab::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsTab::onResetClicked);
}

void SettingsTab::loadSettings()
{
    // Load default settings (would be loaded from daemon in real implementation)
    m_cpuEnabled->setChecked(true);
    m_cpuThreshold->setValue(90);
    m_cpuDuration->setValue(60);

    m_hangEnabled->setChecked(true);
    m_hangDuration->setValue(30);

    m_memoryEnabled->setChecked(true);
    m_memoryGrowth->setValue(500);
    m_memoryWindow->setValue(5);

    m_normalInterval->setValue(10);
    m_alertInterval->setValue(2);
    m_notificationMethod->setCurrentIndex(0);
}

void SettingsTab::onApplyClicked()
{
    // TODO: Send settings to daemon
    emit settingsChanged();
}

void SettingsTab::onResetClicked()
{
    loadSettings();
}
