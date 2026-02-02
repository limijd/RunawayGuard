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
    void settingsChanged();
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
    void loadSettings();
    void loadGuiSettings();
    void setModified(bool modified);
    QJsonObject collectConfig() const;

    // GUI behavior settings
    QCheckBox *m_stopDaemonOnExit;

    // Detection settings
    QCheckBox *m_cpuEnabled;
    QSpinBox *m_cpuThreshold;
    QSpinBox *m_cpuDuration;

    QCheckBox *m_hangEnabled;
    QSpinBox *m_hangDuration;

    QCheckBox *m_memoryEnabled;
    QSpinBox *m_memoryGrowth;
    QSpinBox *m_memoryWindow;

    // General settings
    QSpinBox *m_normalInterval;
    QSpinBox *m_alertInterval;
    QComboBox *m_notificationMethod;

    QPushButton *m_applyButton;
    QPushButton *m_resetButton;

    // State tracking
    bool m_isModified;
    bool m_isConnected;
    QJsonObject m_originalConfig;
};

#endif
