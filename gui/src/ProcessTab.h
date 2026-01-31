#ifndef PROCESSTAB_H
#define PROCESSTAB_H

#include <QWidget>
#include <QTableWidget>
#include <QJsonArray>
#include <QMenu>
#include <QLineEdit>
#include <QSettings>

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
    void applyRowColors(int row, double cpu, double memory, const QString &state);
    void filterTable(const QString &text);
    QString getSelectedCmdline() const;

    QTableWidget *m_table;
    QMenu *m_contextMenu;
    QLineEdit *m_searchEdit;
};

#endif
