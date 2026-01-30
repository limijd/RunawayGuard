#ifndef SETTINGSTAB_H
#define SETTINGSTAB_H

#include <QWidget>

class SettingsTab : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsTab(QWidget *parent = nullptr);
private:
    void setupUi();
};

#endif
