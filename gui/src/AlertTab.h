#ifndef ALERTTAB_H
#define ALERTTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QJsonArray>
#include <QMenu>

class AlertTab : public QWidget
{
    Q_OBJECT
public:
    explicit AlertTab(QWidget *parent = nullptr);

signals:
    void addWhitelistRequested(const QString &pattern, const QString &matchType);
    void killProcessRequested(int pid, const QString &signal);

public slots:
    void updateAlertList(const QJsonArray &alerts);

private slots:
    void showContextMenu(const QPoint &pos);
    void onAddToWhitelist();
    void onTerminateProcess();
    void onDismissAlert();

private:
    void setupUi();
    int getSelectedPid() const;
    QString getSelectedName() const;

    QTableWidget *m_table;
    QMenu *m_contextMenu;
};

#endif
