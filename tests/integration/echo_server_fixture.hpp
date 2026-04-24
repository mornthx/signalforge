// tests/integration/echo_server_fixture.hpp
#pragma once

#include <QHostAddress>
#include <QObject>
#include <QTcpServer>
#include <cstdint>

namespace signalforge::test {

/// In-process TCP echo server for TcpDriver integration tests. Listens
/// on a caller-specified address (default 127.0.0.1) and an
/// OS-assigned ephemeral port. Each incoming connection is accepted and
/// mirrors readAll() back to the peer until it disconnects. `port()` is
/// only valid after `listen()` returns true.
class EchoServer : public QObject {
    Q_OBJECT

public:
    explicit EchoServer(QObject* parent = nullptr);
    ~EchoServer() override;

    /// Start listening. Returns false if the OS refuses the bind.
    bool listen(const QHostAddress& addr = QHostAddress(QHostAddress::LocalHost), quint16 port = 0);

    [[nodiscard]] quint16 port() const noexcept;

    /// Abruptly disconnect all connected clients. Used to simulate
    /// peer-side drop so the driver surfaces ResourceLost.
    void closeAllClients();

private:
    QTcpServer* server_;
};

}  // namespace signalforge::test
