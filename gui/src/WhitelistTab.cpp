#include "WhitelistTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QJsonObject>

WhitelistTab::WhitelistTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_patternEdit(new QLineEdit(this))
    , m_matchTypeCombo(new QComboBox(this))
    , m_addButton(new QPushButton(tr("Add"), this))
    , m_removeButton(new QPushButton(tr("Remove"), this))
{
    setupUi();
}

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

void WhitelistTab::updateWhitelistDisplay(const QJsonArray &whitelist)
{
    m_table->setRowCount(whitelist.size());
    for (int i = 0; i < whitelist.size(); ++i) {
        QJsonObject entry = whitelist[i].toObject();

        auto *patternItem = new QTableWidgetItem(entry["pattern"].toString());
        patternItem->setData(Qt::UserRole, entry["id"].toInt());  // Store ID for removal
        m_table->setItem(i, 0, patternItem);

        m_table->setItem(i, 1, new QTableWidgetItem(entry["match_type"].toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(entry["reason"].toString()));
    }
}

void WhitelistTab::addEntry(const QString &pattern, const QString &matchType)
{
    m_patternEdit->setText(pattern);
    for (int i = 0; i < m_matchTypeCombo->count(); ++i) {
        if (m_matchTypeCombo->itemData(i).toString() == matchType) {
            m_matchTypeCombo->setCurrentIndex(i);
            break;
        }
    }
}

void WhitelistTab::onAddClicked()
{
    QString pattern = m_patternEdit->text().trimmed();
    if (pattern.isEmpty()) return;

    QString matchType = m_matchTypeCombo->currentData().toString();
    emit addWhitelistRequested(pattern, matchType);
    m_patternEdit->clear();
}

void WhitelistTab::onRemoveClicked()
{
    auto items = m_table->selectedItems();
    if (items.isEmpty()) return;

    int row = items.first()->row();
    int id = m_table->item(row, 0)->data(Qt::UserRole).toInt();
    if (id > 0) {
        emit removeWhitelistRequested(id);
    }
}
