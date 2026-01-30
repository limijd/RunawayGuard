#ifndef PROCESSTAB_H
#define PROCESSTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QJsonArray>
#include <QMenu>

class ProcessTab : public QWidget
{
    Q_OBJECT
public:
    explicit ProcessTab(QWidget *parent = nullptr);

signals:
    void killProcessRequested(int pid, const QString &signal);
    void addWhitelistRequested(const QString &pattern, const QString &matchType);

public slots:
    void updateProcessList(const QJsonArray &processes);

private slots:
    void showContextMenu(const QPoint &pos);
    void onTerminateProcess();
    void onKillProcess();
    void onStopProcess();
    void onContinueProcess();
    void onAddToWhitelist();

private:
    void setupUi();
    int getSelectedPid() const;
    QString getSelectedName() const;

    QTableWidget *m_table;
    QMenu *m_contextMenu;
};

#endif
