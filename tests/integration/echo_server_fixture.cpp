// tests/integration/echo_server_fixture.cpp
#include "tests/integration/echo_server_fixture.hpp"

#include <QTcpSocket>

namespace signalforge::test {

EchoServer::EchoServer(QObject* parent) : QObject(parent), server_(new QTcpServer(this)) {
    QObject::connect(server_, &QTcpServer::newConnection, this, [this]() {
        while (server_->hasPendingConnections()) {
            auto* client = server_->nextPendingConnection();
            QObject::connect(client, &QTcpSocket::readyRead, client, [client]() {
                const QByteArray chunk = client->readAll();
                if (!chunk.isEmpty()) {
                    client->write(chunk);
                }
            });
            QObject::connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
        }
    });
}

EchoServer::~EchoServer() = default;

bool EchoServer::listen(const QHostAddress& addr, quint16 port) {
    return server_->listen(addr, port);
}

quint16 EchoServer::port() const noexcept {
    return server_->serverPort();
}

void EchoServer::closeAllClients() {
    const auto children = server_->children();
    for (QObject* child : children) {
        if (auto* sock = qobject_cast<QTcpSocket*>(child)) {
            sock->abort();
            sock->deleteLater();
        }
    }
}

}  // namespace signalforge::test
