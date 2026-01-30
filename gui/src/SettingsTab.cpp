#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QLabel>

SettingsTab::SettingsTab(QWidget *parent) : QWidget(parent) { setupUi(); }

void SettingsTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Settings will be implemented here")));
    layout->addStretch();
}
