#ifndef PROCESSTAB_H
#define PROCESSTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QJsonArray>

class ProcessTab : public QWidget
{
    Q_OBJECT
public:
    explicit ProcessTab(QWidget *parent = nullptr);
public slots:
    void updateProcessList(const QJsonArray &processes);
private:
    void setupUi();
    QTableWidget *m_table;
};

#endif
