#include "WhitelistTab.h"
#include <QVBoxLayout>
#include <QHeaderView>

WhitelistTab::WhitelistTab(QWidget *parent) : QWidget(parent), m_table(new QTableWidget(this)) { setupUi(); }

void WhitelistTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Pattern"), tr("Match Type"), tr("Reason")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);
}
