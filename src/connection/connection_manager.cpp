// src/connection/connection_manager.cpp
//
// S3: CRUD + lifecycle dispatch + signal forwarding. yaml
// persistence is filled in by S4; the auto-save hook here calls
// saveConfigFile() which is a stub until S4.

#include "connection/connection_manager.hpp"

#include "decode/decoder_registrar.hpp"
#include "observability/logging.hpp"

#include <QStandardPaths>
#include <random>

namespace signalforge::connection {

ConnectionManager::ConnectionManager(signalforge::decoder::DecoderRegistrar& decoderRegistrar, QObject* parent)
    : QObject(parent), decoderRegistrar_(&decoderRegistrar) {}

ConnectionManager::~ConnectionManager() = default;

QString ConnectionManager::addConnection(const ConnectionConfig& config) {
    auto resolved = config;
    if (resolved.id.isEmpty()) {
        resolved.id = generateId(QStringLiteral("conn"));
        if (resolved.id.isEmpty()) {
            SF_LOG_ERROR("ConnectionManager::addConnection: failed to generate unique id");
            return {};
        }
    } else if (connections_.find(resolved.id) != connections_.end()) {
        SF_LOG_WARN("ConnectionManager::addConnection: id '{}' already exists", resolved.id.toStdString());
        return {};
    }

    auto conn = std::make_unique<Connection>(resolved, this);
    Connection& ref = *conn;
    const QString id = resolved.id;
    insertConnection(id, std::move(conn));
    wireConnection(id, ref);

    Q_EMIT connectionAdded(id);
    (void)autoSave();
    return id;
}

bool ConnectionManager::editConnection(const QString& id, const ConnectionConfig& newConfig) {
    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return false;
    }
    Connection* conn = it->second.get();
    if (!conn) {
        return false;
    }
    if (conn->state() != Connection::State::Idle) {
        return false;
    }
    auto adjusted = newConfig;
    adjusted.id = id;  // id is immutable across edit
    if (!conn->setConfig(adjusted)) {
        return false;
    }
    // setConfig rebuilds the underlying driver but the Connection
    // QObject itself is the same — its stateChanged/errorOccurred
    // signals are still wired from addConnection().
    (void)autoSave();
    return true;
}

bool ConnectionManager::removeConnection(const QString& id) {
    auto it = connections_.find(id);
    if (it == connections_.end()) {
        return false;
    }
    if (it->second && it->second->state() != Connection::State::Idle) {
        return false;
    }
    connections_.erase(it);
    orderedIds_.removeAll(id);
    Q_EMIT connectionRemoved(id);
    (void)autoSave();
    return true;
}

bool ConnectionManager::connectConnection(const QString& id) {
    auto it = connections_.find(id);
    if (it == connections_.end() || !it->second) {
        return false;
    }
    return it->second->connectDriver();
}

bool ConnectionManager::disconnectConnection(const QString& id) {
    auto it = connections_.find(id);
    if (it == connections_.end() || !it->second) {
        return false;
    }
    return it->second->disconnectDriver();
}

void ConnectionManager::connectAll() {
    for (const QString& id : orderedIds_) {
        auto it = connections_.find(id);
        if (it == connections_.end() || !it->second) {
            continue;
        }
        if (it->second->state() == Connection::State::Idle || it->second->state() == Connection::State::Error) {
            (void)it->second->connectDriver();
        }
    }
}

void ConnectionManager::disconnectAll() {
    for (const QString& id : orderedIds_) {
        auto it = connections_.find(id);
        if (it == connections_.end() || !it->second) {
            continue;
        }
        if (it->second->state() != Connection::State::Idle) {
            (void)it->second->disconnectDriver();
        }
    }
}

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
    return true;
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
    return {};
}

void ConnectionManager::wireConnection(const QString& id, Connection& conn) {
    // Forward per-Connection signals through manager-level
    // signals tagged with the id. DirectConnection (default) is
    // safe since Connection lives on the same thread as the
    // manager. Called once per Connection (from addConnection);
    // setConfig keeps the same Connection object across edit so
    // no re-wiring is needed.
    connect(&conn, &Connection::stateChanged, this,
            [this, id](Connection::State s) { Q_EMIT connectionStateChanged(id, s); });
    connect(&conn, &Connection::errorOccurred, this,
            [this, id](const QString& msg) { Q_EMIT connectionError(id, msg); });
}

bool ConnectionManager::autoSave() const {
    if (configPath_.isEmpty()) {
        return true;  // No path configured yet; nothing to save.
    }
    return saveConfigFile(configPath_);
}

void ConnectionManager::insertConnection(const QString& id, std::unique_ptr<Connection> conn) {
    if (!conn || id.isEmpty()) {
        return;
    }
    if (!orderedIds_.contains(id)) {
        orderedIds_.push_back(id);
    }
    connections_[id] = std::move(conn);
}

}  // namespace signalforge::connection
