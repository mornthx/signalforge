// src/connection/connection_manager.hpp
#pragma once

#include "connection/connection.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>

namespace signalforge::decoder {
class DecoderRegistrar;
}  // namespace signalforge::decoder

namespace signalforge::connection {

/// Owns N `Connection` instances, persists the set to yaml, and
/// fans out per-connection state changes as manager-level Qt
/// signals.
///
/// Lifecycle:
///   - Constructed in `MainWindow` with a reference to the
///     `DecoderRegistrar` (for schema lookup at connect-time;
///     also kept as a non-owning pointer for re-binding on edit).
///   - `loadConfigFile(path)` is called on app start; produces
///     `Connection` instances all in Idle. `connectStartupConnections()`
///     then connects only entries explicitly marked for startup.
///   - User actions (connect / disconnect / add / edit / remove)
///     mutate state. Auto-save fires on every CRUD.
///
/// Threading: main-thread-only. The owned `Connection` instances
/// each own a driver that may run IO on a background thread (M3
/// topology); state changes are reported back via Qt signals on
/// the main thread.
///
/// Freeze scope: this class, its public method set, and its
/// signals are frozen at M9 close.
class ConnectionManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionManager)

public:
    /// `decoderRegistrar` must outlive this manager.
    explicit ConnectionManager(signalforge::decoder::DecoderRegistrar& decoderRegistrar, QObject* parent = nullptr);
    ~ConnectionManager() override;

    // ── CRUD ────────────────────────────────────────────────

    /// Add a new connection. If `config.id` is empty, an id is
    /// auto-generated. Persists to the configured yaml path.
    /// Returns the resolved id.
    [[nodiscard]] QString addConnection(const ConnectionConfig& config);

    /// Replace an existing connection's config. Only works in
    /// Idle (returns `false` otherwise). Persists on success.
    [[nodiscard]] bool editConnection(const QString& id, const ConnectionConfig& newConfig);

    /// Remove a connection. Only works in Idle (returns `false`
    /// otherwise). Persists on success.
    [[nodiscard]] bool removeConnection(const QString& id);

    // ── Lifecycle ───────────────────────────────────────────

    /// Initiate connect for a connection. Forwards to
    /// `Connection::connectDriver()`. Returns `false` if the id
    /// is unknown or the connection cannot be connected.
    [[nodiscard]] bool connectConnection(const QString& id);

    /// Initiate disconnect for a connection. Returns `false` if
    /// the id is unknown or the connection cannot be disconnected.
    [[nodiscard]] bool disconnectConnection(const QString& id);

    /// Connect every Idle connection.
    void connectAll();

    /// Disconnect every non-Idle connection.
    void disconnectAll();

    /// Connect connections whose persisted config opts into startup
    /// auto-connect. Returns the number of connect attempts accepted by
    /// the underlying `Connection`.
    [[nodiscard]] int connectStartupConnections();

    // ── Lookup ──────────────────────────────────────────────

    /// Returns the connection with the given id, or `nullptr` if
    /// unknown. Pointer is valid until the connection is removed.
    [[nodiscard]] Connection* connection(const QString& id) const;

    /// Stable list of connection ids in insertion order.
    [[nodiscard]] QStringList connectionIds() const;

    [[nodiscard]] std::size_t connectionCount() const noexcept;
    [[nodiscard]] std::size_t connectedCount() const noexcept;
    [[nodiscard]] std::size_t erroredCount() const noexcept;

    // ── Persistence ─────────────────────────────────────────

    /// Load connections from `path`. On parse error, leaves the
    /// manager empty and returns `false`. On missing file,
    /// likewise (with INFO log). On success, sets the auto-save
    /// path to `path`.
    [[nodiscard]] bool loadConfigFile(const QString& path);

    /// Save the current connection set to `path`.
    [[nodiscard]] bool saveConfigFile(const QString& path) const;

    /// Returns the platform-canonical default config path:
    /// `<QStandardPaths::AppConfigLocation>/connections.yaml`.
    [[nodiscard]] static QString defaultConfigPath();

signals:
    void connectionAdded(const QString& id);
    void connectionRemoved(const QString& id);
    void connectionStateChanged(const QString& id, signalforge::connection::Connection::State newState);
    void connectionError(const QString& id, const QString& errorMessage);
    void configurationSaveStateChanged(bool saved, const QString& path, const QString& message);

private:
    QString generateId(const QString& prefix) const;
    void wireConnection(const QString& id, Connection& conn);
    bool autoSave();
    void insertConnection(const QString& id, std::unique_ptr<Connection> conn);

    signalforge::decoder::DecoderRegistrar* decoderRegistrar_;
    std::unordered_map<QString, std::unique_ptr<Connection>> connections_;
    QStringList orderedIds_;  ///< Insertion order for stable iteration
    QString configPath_;
};

}  // namespace signalforge::connection
