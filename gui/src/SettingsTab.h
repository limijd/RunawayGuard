#ifndef SETTINGSTAB_H
#define SETTINGSTAB_H

#include <QWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>

class SettingsTab : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsTab(QWidget *parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onApplyClicked();
    void onResetClicked();

private:
    void setupUi();
    void loadSettings();

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
};

#endif
