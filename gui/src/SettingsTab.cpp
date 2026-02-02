#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
    , m_stopDaemonOnExit(new QCheckBox(tr("Stop daemon when GUI exits"), this))
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
    , m_isModified(false)
    , m_isConnected(false)
{
    setupUi();
    loadGuiSettings();
    loadSettings();
    setConnected(false);
}

void SettingsTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // GUI Behavior Group
    auto *guiGroup = new QGroupBox(tr("GUI Behavior"), this);
    auto *guiLayout = new QVBoxLayout(guiGroup);
    guiLayout->setContentsMargins(12, 12, 12, 12);
    guiLayout->addWidget(m_stopDaemonOnExit);
    mainLayout->addWidget(guiGroup);

    // CPU Detection Group
    auto *cpuGroup = new QGroupBox(tr("CPU High Detection"), this);
    auto *cpuLayout = new QFormLayout(cpuGroup);
    cpuLayout->setContentsMargins(12, 12, 12, 12);
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
    hangLayout->setContentsMargins(12, 12, 12, 12);
    hangLayout->addRow(m_hangEnabled);
    m_hangDuration->setRange(1, 3600);
    m_hangDuration->setSuffix(tr(" sec"));
    hangLayout->addRow(tr("Duration:"), m_hangDuration);
    mainLayout->addWidget(hangGroup);

    // Memory Detection Group
    auto *memoryGroup = new QGroupBox(tr("Memory Leak Detection"), this);
    auto *memoryLayout = new QFormLayout(memoryGroup);
    memoryLayout->setContentsMargins(12, 12, 12, 12);
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
    generalLayout->setContentsMargins(12, 12, 12, 12);
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

    // Button connections
    connect(m_applyButton, &QPushButton::clicked, this, &SettingsTab::onApplyClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsTab::onResetClicked);

    // Connect all settings widgets to track modifications
    connect(m_cpuEnabled, &QCheckBox::toggled, this, &SettingsTab::onSettingChanged);
    connect(m_cpuThreshold, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    connect(m_cpuDuration, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);

    connect(m_hangEnabled, &QCheckBox::toggled, this, &SettingsTab::onSettingChanged);
    connect(m_hangDuration, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);

    connect(m_memoryEnabled, &QCheckBox::toggled, this, &SettingsTab::onSettingChanged);
    connect(m_memoryGrowth, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    connect(m_memoryWindow, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);

    connect(m_normalInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    connect(m_alertInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsTab::onSettingChanged);
    connect(m_notificationMethod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsTab::onSettingChanged);
}

void SettingsTab::loadGuiSettings()
{
    QSettings settings("RunawayGuard", "GUI");
    bool stopDaemonOnExit = settings.value("manageDaemonLifecycle", true).toBool();
    m_stopDaemonOnExit->setChecked(stopDaemonOnExit);

    // Connect checkbox to save immediately when changed
    connect(m_stopDaemonOnExit, &QCheckBox::toggled, this, [](bool checked) {
        QSettings settings("RunawayGuard", "GUI");
        settings.setValue("manageDaemonLifecycle", checked);
    });
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

void SettingsTab::loadConfig(const QJsonObject &config)
{
    // Store original config for reset functionality
    m_originalConfig = config;

    // Block signals while loading to avoid triggering onSettingChanged
    m_cpuEnabled->blockSignals(true);
    m_cpuThreshold->blockSignals(true);
    m_cpuDuration->blockSignals(true);
    m_hangEnabled->blockSignals(true);
    m_hangDuration->blockSignals(true);
    m_memoryEnabled->blockSignals(true);
    m_memoryGrowth->blockSignals(true);
    m_memoryWindow->blockSignals(true);
    m_normalInterval->blockSignals(true);
    m_alertInterval->blockSignals(true);
    m_notificationMethod->blockSignals(true);

    // Load CPU High Detection settings
    if (config.contains("cpu_high")) {
        QJsonObject cpuHigh = config["cpu_high"].toObject();
        m_cpuEnabled->setChecked(cpuHigh["enabled"].toBool(true));
        m_cpuThreshold->setValue(cpuHigh["threshold_percent"].toInt(90));
        m_cpuDuration->setValue(cpuHigh["duration_seconds"].toInt(60));
    }

    // Load Hang Detection settings
    if (config.contains("hang")) {
        QJsonObject hang = config["hang"].toObject();
        m_hangEnabled->setChecked(hang["enabled"].toBool(true));
        m_hangDuration->setValue(hang["duration_seconds"].toInt(30));
    }

    // Load Memory Leak Detection settings
    if (config.contains("memory_leak")) {
        QJsonObject memoryLeak = config["memory_leak"].toObject();
        m_memoryEnabled->setChecked(memoryLeak["enabled"].toBool(true));
        m_memoryGrowth->setValue(memoryLeak["growth_threshold_mb"].toInt(500));
        m_memoryWindow->setValue(memoryLeak["window_minutes"].toInt(5));
    }

    // Load General settings
    if (config.contains("general")) {
        QJsonObject general = config["general"].toObject();
        m_normalInterval->setValue(general["sample_interval_normal"].toInt(10));
        m_alertInterval->setValue(general["sample_interval_alert"].toInt(2));

        QString notificationMethod = general["notification_method"].toString("both");
        int index = m_notificationMethod->findData(notificationMethod);
        if (index >= 0) {
            m_notificationMethod->setCurrentIndex(index);
        }
    }

    // Restore signals
    m_cpuEnabled->blockSignals(false);
    m_cpuThreshold->blockSignals(false);
    m_cpuDuration->blockSignals(false);
    m_hangEnabled->blockSignals(false);
    m_hangDuration->blockSignals(false);
    m_memoryEnabled->blockSignals(false);
    m_memoryGrowth->blockSignals(false);
    m_memoryWindow->blockSignals(false);
    m_normalInterval->blockSignals(false);
    m_alertInterval->blockSignals(false);
    m_notificationMethod->blockSignals(false);

    // Reset modified state after loading
    setModified(false);
}

void SettingsTab::setConnected(bool connected)
{
    m_isConnected = connected;

    // Enable/disable all settings widgets based on connection state
    m_cpuEnabled->setEnabled(connected);
    m_cpuThreshold->setEnabled(connected);
    m_cpuDuration->setEnabled(connected);

    m_hangEnabled->setEnabled(connected);
    m_hangDuration->setEnabled(connected);

    m_memoryEnabled->setEnabled(connected);
    m_memoryGrowth->setEnabled(connected);
    m_memoryWindow->setEnabled(connected);

    m_normalInterval->setEnabled(connected);
    m_alertInterval->setEnabled(connected);
    m_notificationMethod->setEnabled(connected);

    // Buttons depend on both connection state and modification state
    m_applyButton->setEnabled(connected && m_isModified);
    m_resetButton->setEnabled(connected && m_isModified);
}

void SettingsTab::onSettingChanged()
{
    setModified(true);
}

void SettingsTab::setModified(bool modified)
{
    m_isModified = modified;

    // Update button states
    m_applyButton->setEnabled(m_isConnected && m_isModified);
    m_resetButton->setEnabled(m_isConnected && m_isModified);
}

QJsonObject SettingsTab::collectConfig() const
{
    QJsonObject config;

    // CPU High Detection
    QJsonObject cpuHigh;
    cpuHigh["enabled"] = m_cpuEnabled->isChecked();
    cpuHigh["threshold_percent"] = m_cpuThreshold->value();
    cpuHigh["duration_seconds"] = m_cpuDuration->value();
    config["cpu_high"] = cpuHigh;

    // Hang Detection
    QJsonObject hang;
    hang["enabled"] = m_hangEnabled->isChecked();
    hang["duration_seconds"] = m_hangDuration->value();
    config["hang"] = hang;

    // Memory Leak Detection
    QJsonObject memoryLeak;
    memoryLeak["enabled"] = m_memoryEnabled->isChecked();
    memoryLeak["growth_threshold_mb"] = m_memoryGrowth->value();
    memoryLeak["window_minutes"] = m_memoryWindow->value();
    config["memory_leak"] = memoryLeak;

    // General settings
    QJsonObject general;
    general["sample_interval_normal"] = m_normalInterval->value();
    general["sample_interval_alert"] = m_alertInterval->value();
    general["notification_method"] = m_notificationMethod->currentData().toString();
    config["general"] = general;

    return config;
}

void SettingsTab::onApplyClicked()
{
    QJsonObject config = collectConfig();
    emit configUpdateRequested(config);
    emit settingsChanged();
    setModified(false);
}

void SettingsTab::onResetClicked()
{
    if (!m_originalConfig.isEmpty()) {
        loadConfig(m_originalConfig);
    } else {
        loadSettings();
        setModified(false);
    }
}
