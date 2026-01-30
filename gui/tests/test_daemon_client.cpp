#include <QTest>
#include <QSignalSpy>
#include "DaemonClient.h"

class TestDaemonClient : public QObject
{
    Q_OBJECT

private slots:
    void testInitialization()
    {
        DaemonClient client;
        // Client should be created without crashing
        QVERIFY(true);
    }

    void testConnectSignal()
    {
        DaemonClient client;
        QSignalSpy spy(&client, &DaemonClient::connected);
        // Just verify signal can be connected
        QVERIFY(spy.isValid());
    }

    void testDisconnectSignal()
    {
        DaemonClient client;
        QSignalSpy spy(&client, &DaemonClient::disconnected);
        QVERIFY(spy.isValid());
    }

    void testAlertReceivedSignal()
    {
        DaemonClient client;
        QSignalSpy spy(&client, &DaemonClient::alertReceived);
        QVERIFY(spy.isValid());
    }

    void testStatusReceivedSignal()
    {
        DaemonClient client;
        QSignalSpy spy(&client, &DaemonClient::statusReceived);
        QVERIFY(spy.isValid());
    }
};

QTEST_MAIN(TestDaemonClient)
#include "test_daemon_client.moc"
