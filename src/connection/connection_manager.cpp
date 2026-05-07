// src/connection/connection_manager.cpp
//
// S1: minimal scaffolding. CRUD + signals + persistence land in
// S3 + S4. S1 establishes the freeze-surface header and a buildable
// stub.

#include "connection/connection_manager.hpp"

#include "decode/decoder_registrar.hpp"
#include "observability/logging.hpp"

#include <QStandardPaths>
#include <random>

namespace signalforge::connection {

ConnectionManager::ConnectionManager(signalforge::decoder::DecoderRegistrar& decoderRegistrar, QObject* parent)
    : QObject(parent), decoderRegistrar_(&decoderRegistrar) {}

ConnectionManager::~ConnectionManager() = default;

QString ConnectionManager::addConnection(const ConnectionConfig& /*config*/) {
    // S3 fills in.
    SF_LOG_DEBUG("ConnectionManager::addConnection() stub");
    return {};
}

bool ConnectionManager::editConnection(const QString& /*id*/, const ConnectionConfig& /*newConfig*/) {
    return false;
}

bool ConnectionManager::removeConnection(const QString& /*id*/) {
    return false;
}

bool ConnectionManager::connectConnection(const QString& /*id*/) {
    return false;
}

bool ConnectionManager::disconnectConnection(const QString& /*id*/) {
    return false;
}

void ConnectionManager::connectAll() {}

void ConnectionManager::disconnectAll() {}

Connection* ConnectionManager::connection(const QString& id) const {
    auto it = connections_.find(id);
    return it == connections_.end() ? nullptr : it->second.get();
}

QStringList ConnectionManager::connectionIds() const {
    return orderedIds_;
}

std::size_t ConnectionManager::connectionCount() const noexcept {
    return connections_.size();
}

std::size_t ConnectionManager::connectedCount() const noexcept {
    std::size_t n = 0;
    for (const auto& [id, conn] : connections_) {
        if (conn && conn->state() == Connection::State::Connected) {
            ++n;
        }
    }
    return n;
}

std::size_t ConnectionManager::erroredCount() const noexcept {
    std::size_t n = 0;
    for (const auto& [id, conn] : connections_) {
        if (conn && conn->state() == Connection::State::Error) {
            ++n;
        }
    }
    return n;
}

bool ConnectionManager::loadConfigFile(const QString& /*path*/) {
    // S4 fills in.
    return false;
}

bool ConnectionManager::saveConfigFile(const QString& /*path*/) const {
    // S4 fills in.
    return false;
}

QString ConnectionManager::defaultConfigPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return base + QStringLiteral("/connections.yaml");
}

QString ConnectionManager::generateId(const QString& prefix) const {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    for (int attempt = 0; attempt < 8; ++attempt) {
        QString candidate = QStringLiteral("%1-%2")
                                .arg(prefix.isEmpty() ? QStringLiteral("conn") : prefix)
                                .arg(dist(rng), 8, 16, QLatin1Char('0'));
        if (connections_.find(candidate) == connections_.end()) {
            return candidate;
        }
    }
    // Astronomically unlikely; signal failure with empty.
    return {};
}

void ConnectionManager::wireConnection(const QString& /*id*/, Connection& /*conn*/) {
    // S3 fills in.
}

bool ConnectionManager::autoSave() const {
    if (configPath_.isEmpty()) {
        return true;  // Nothing to save against; treat as success.
    }
    return saveConfigFile(configPath_);
}

void ConnectionManager::insertConnection(const QString& id, std::unique_ptr<Connection> conn) {
    if (!conn || id.isEmpty()) {
        return;
    }
    orderedIds_.push_back(id);
    connections_[id] = std::move(conn);
}

}  // namespace signalforge::connection
