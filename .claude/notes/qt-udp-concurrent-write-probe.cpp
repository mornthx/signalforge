// Identify which of the two concurrent writes fails.
#include <QCoreApplication>
#include <QHostAddress>
#include <QObject>
#include <QThread>
#include <QUdpSocket>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

static std::mutex g_out_mu;

class Worker : public QObject {
    Q_OBJECT
public:
    QUdpSocket* sock{nullptr};
    quint16 peerPort{0};
    const char* tag{"?"};
    int iter{0};
public slots:
    void bindSelf() {
        sock = new QUdpSocket(this);
        sock->bind(QHostAddress(QHostAddress::LocalHost), 0);
    }
    void sendTo() {
        QByteArray payload(64, '\xAA');
        qint64 n = sock->writeDatagram(payload, QHostAddress(QHostAddress::LocalHost), peerPort);
        std::lock_guard<std::mutex> lk(g_out_mu);
        fprintf(stdout, "iter=%d tag=%s result=%s err=%d lp=%u pp=%u\n",
                iter, tag, n >= 0 ? "OK" : "FAIL",
                (int)sock->error(), sock->localPort(), peerPort);
        fflush(stdout);
    }
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    for (int iter = 0; iter < 8; ++iter) {
        QThread tA, tB;
        Worker wA, wB;
        wA.tag = "A"; wB.tag = "B";
        wA.iter = iter; wB.iter = iter;
        wA.moveToThread(&tA); wB.moveToThread(&tB);
        tA.start(); tB.start();
        QMetaObject::invokeMethod(&wA, "bindSelf", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(&wB, "bindSelf", Qt::BlockingQueuedConnection);
        wA.peerPort = wB.sock->localPort();
        wB.peerPort = wA.sock->localPort();
        QMetaObject::invokeMethod(&wA, "sendTo", Qt::QueuedConnection);
        QMetaObject::invokeMethod(&wB, "sendTo", Qt::QueuedConnection);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        tA.quit(); tB.quit(); tA.wait(); tB.wait();
    }
    return 0;
}
#include "qt_udp_probe3.moc"
