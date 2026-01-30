#ifndef WHITELISTTAB_H
#define WHITELISTTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QJsonArray>

class WhitelistTab : public QWidget
{
    Q_OBJECT
public:
    explicit WhitelistTab(QWidget *parent = nullptr);

signals:
    void addWhitelistRequested(const QString &pattern, const QString &matchType);
    void removeWhitelistRequested(int id);

public slots:
    void updateWhitelistDisplay(const QJsonArray &whitelist);
    void addEntry(const QString &pattern, const QString &matchType);

private slots:
    void onAddClicked();
    void onRemoveClicked();

private:
    void setupUi();

    QTableWidget *m_table;
    QLineEdit *m_patternEdit;
    QComboBox *m_matchTypeCombo;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
};

#endif
