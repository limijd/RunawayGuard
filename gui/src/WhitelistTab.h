#ifndef WHITELISTTAB_H
#define WHITELISTTAB_H

#include <QWidget>
#include <QTableWidget>

class WhitelistTab : public QWidget
{
    Q_OBJECT
public:
    explicit WhitelistTab(QWidget *parent = nullptr);
private:
    void setupUi();
    QTableWidget *m_table;
};

#endif
