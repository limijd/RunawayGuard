#ifndef ALERTTAB_H
#define ALERTTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QJsonArray>

class AlertTab : public QWidget
{
    Q_OBJECT
public:
    explicit AlertTab(QWidget *parent = nullptr);
public slots:
    void updateAlertList(const QJsonArray &alerts);
private:
    void setupUi();
    QTableWidget *m_table;
};

#endif
