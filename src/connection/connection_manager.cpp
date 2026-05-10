// src/connection/connection_manager.cpp
//
// S3: CRUD + lifecycle dispatch + signal forwarding. yaml
// persistence is filled in by S4; the auto-save hook here calls
// saveConfigFile() which is a stub until S4.

#include "connection/connection_manager.hpp"

#include "decode/decoder_registrar.hpp"
#include "observability/logging.hpp"

#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <fstream>
#include <random>
#include <sstream>
#include <yaml-cpp/yaml.h>

namespace signalforge::connection {

namespace {

// ADR-008 helper. Maps a ConnectionConfig's driverType + decoderSchemaId
// onto a `setSchemaForDriverType` call. Empty `decoderSchemaId` →
// empty schemaPath (clears the registrar's map entry for this type).
//
// V1 schema-id-to-path convention: `examples/schemas/<id>.yaml`
// (matches `MainWindow::enumerateAvailableSchemaIds`).
QString driverTypeToYamlInternal(DriverType t) {
    switch (t) {
    case DriverType::Serial:
        return QStringLiteral("serial");
    case DriverType::Tcp:
        return QStringLiteral("tcp");
    case DriverType::Udp:
        return QStringLiteral("udp");
    case DriverType::Replay:
        return QStringLiteral("replay");
    }
    return QStringLiteral("serial");
}

QString resolveSchemaPath(const QString& decoderSchemaId) {
    if (decoderSchemaId.isEmpty()) {
        return QString();
    }
    return QStringLiteral("examples/schemas/") + decoderSchemaId + QStringLiteral(".yaml");
}

// ADR-008 wire-up: map a ConnectionConfig onto a
// `setSchemaForDriverType` call. Free helper in anonymous
// namespace so we don't add a method to M9-frozen
// ConnectionManager::* (ADR-008 only authorizes the additive
// method on M5-frozen DecoderRegistrar).
void applyDecoderSchemaForConfig(signalforge::decoder::DecoderRegistrar* registrar, const ConnectionConfig& cfg) {
    if (registrar == nullptr) {
        return;
    }
    registrar->setSchemaForDriverType(driverTypeToYamlInternal(cfg.driverType), resolveSchemaPath(cfg.decoderSchemaId));
}

}  // namespace

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

    // ADR-008: wire the per-connection decoderSchemaId to the
    // DecoderRegistrar's runtime map so SchemaDecoder pipelines
    // attach with the right schema.
    applyDecoderSchemaForConfig(decoderRegistrar_, resolved);

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
    // ADR-008: refresh the registrar's per-driver-type schema map
    // so future pipelineAttached events for this driver type use
    // the (possibly changed) decoderSchemaId.
    applyDecoderSchemaForConfig(decoderRegistrar_, adjusted);
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
    // ADR-008: capture the driver type before erasing so we can
    // clear the registrar entry. (Per ADR §"Known limitations":
    // if multiple connections of the same type exist, this clear
    // removes the entry for all of them; survivors must be
    // re-registered via edit.)
    const auto removedType = it->second ? it->second->config().driverType : DriverType::Serial;
    connections_.erase(it);
    orderedIds_.removeAll(id);
    if (decoderRegistrar_ != nullptr) {
        decoderRegistrar_->setSchemaForDriverType(driverTypeToYamlInternal(removedType), QString());
    }
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

namespace {

constexpr int kSchemaVersion = 1;

const char* driverTypeToYaml(DriverType t) {
    switch (t) {
    case DriverType::Serial:
        return "serial";
    case DriverType::Tcp:
        return "tcp";
    case DriverType::Udp:
        return "udp";
    case DriverType::Replay:
        return "replay";
    }
    return "serial";
}

std::optional<DriverType> driverTypeFromYaml(const std::string& s) {
    if (s == "serial")
        return DriverType::Serial;
    if (s == "tcp")
        return DriverType::Tcp;
    if (s == "udp")
        return DriverType::Udp;
    if (s == "replay")
        return DriverType::Replay;
    return std::nullopt;
}

YAML::Binary toBinary(const QByteArray& payload) {
    return YAML::Binary(reinterpret_cast<const unsigned char*>(payload.constData()), payload.size());
}

QByteArray fromBinary(const YAML::Binary& b) {
    return QByteArray(reinterpret_cast<const char*>(b.data()), static_cast<int>(b.size()));
}

void emitDriverConfig(YAML::Emitter& out, const ConnectionConfig& cfg) {
    out << YAML::BeginMap;
    switch (cfg.driverType) {
    case DriverType::Serial: {
        const auto& s = std::get<signalforge::drivers::SerialConfig>(cfg.driverConfig);
        out << YAML::Key << "device" << YAML::Value << s.device.toStdString();
        out << YAML::Key << "baudRate" << YAML::Value << s.baudRate;
        out << YAML::Key << "dataBits" << YAML::Value << s.dataBits;
        out << YAML::Key << "parity" << YAML::Value << s.parity.toStdString();
        out << YAML::Key << "stopBits" << YAML::Value << s.stopBits;
        out << YAML::Key << "flowControl" << YAML::Value << s.flowControl.toStdString();
        break;
    }
    case DriverType::Tcp: {
        const auto& t = std::get<signalforge::drivers::TcpConfig>(cfg.driverConfig);
        out << YAML::Key << "host" << YAML::Value << t.host.toStdString();
        out << YAML::Key << "port" << YAML::Value << static_cast<int>(t.port);
        out << YAML::Key << "connectTimeout" << YAML::Value << static_cast<long long>(t.connectTimeout.count());
        break;
    }
    case DriverType::Udp: {
        const auto& u = std::get<signalforge::drivers::UdpConfig>(cfg.driverConfig);
        out << YAML::Key << "localBindAddress" << YAML::Value << u.localBindAddress.toStdString();
        out << YAML::Key << "localBindPort" << YAML::Value << static_cast<int>(u.localBindPort);
        out << YAML::Key << "remoteHost" << YAML::Value << u.remoteHost.toStdString();
        out << YAML::Key << "remotePort" << YAML::Value << static_cast<int>(u.remotePort);
        out << YAML::Key << "multicastGroup" << YAML::Value << u.multicastGroup.toStdString();
        out << YAML::Key << "multicastTtl" << YAML::Value << static_cast<unsigned>(u.multicastTtl);
        break;
    }
    case DriverType::Replay: {
        const auto& r = std::get<signalforge::drivers::ReplayConfig>(cfg.driverConfig);
        out << YAML::Key << "sessionFilePath" << YAML::Value << r.sessionFilePath.toStdString();
        out << YAML::Key << "playbackSpeed" << YAML::Value << r.playbackSpeed;
        out << YAML::Key << "loop" << YAML::Value << r.loop;
        break;
    }
    }
    out << YAML::EndMap;
}

bool readDriverConfig(const YAML::Node& node, DriverType type, DriverConfig& outConfig) {
    if (!node.IsMap()) {
        return false;
    }
    switch (type) {
    case DriverType::Serial: {
        signalforge::drivers::SerialConfig s;
        if (!node["device"]) {
            return false;
        }
        s.device = QString::fromStdString(node["device"].as<std::string>());
        if (node["baudRate"])
            s.baudRate = node["baudRate"].as<int>();
        if (node["dataBits"])
            s.dataBits = node["dataBits"].as<int>();
        if (node["parity"])
            s.parity = QString::fromStdString(node["parity"].as<std::string>());
        if (node["stopBits"])
            s.stopBits = node["stopBits"].as<int>();
        if (node["flowControl"])
            s.flowControl = QString::fromStdString(node["flowControl"].as<std::string>());
        outConfig = s;
        return true;
    }
    case DriverType::Tcp: {
        signalforge::drivers::TcpConfig t;
        if (!node["host"] || !node["port"]) {
            return false;
        }
        t.host = QString::fromStdString(node["host"].as<std::string>());
        t.port = static_cast<quint16>(node["port"].as<int>());
        if (node["connectTimeout"]) {
            t.connectTimeout = std::chrono::milliseconds{node["connectTimeout"].as<long long>()};
        }
        outConfig = t;
        return true;
    }
    case DriverType::Udp: {
        signalforge::drivers::UdpConfig u;
        if (node["localBindAddress"])
            u.localBindAddress = QString::fromStdString(node["localBindAddress"].as<std::string>());
        if (node["localBindPort"])
            u.localBindPort = static_cast<quint16>(node["localBindPort"].as<int>());
        if (node["remoteHost"])
            u.remoteHost = QString::fromStdString(node["remoteHost"].as<std::string>());
        if (node["remotePort"])
            u.remotePort = static_cast<quint16>(node["remotePort"].as<int>());
        if (node["multicastGroup"])
            u.multicastGroup = QString::fromStdString(node["multicastGroup"].as<std::string>());
        if (node["multicastTtl"])
            u.multicastTtl = node["multicastTtl"].as<unsigned>();
        outConfig = u;
        return true;
    }
    case DriverType::Replay: {
        signalforge::drivers::ReplayConfig r;
        if (!node["sessionFilePath"]) {
            return false;
        }
        r.sessionFilePath = QString::fromStdString(node["sessionFilePath"].as<std::string>());
        if (node["playbackSpeed"])
            r.playbackSpeed = node["playbackSpeed"].as<double>();
        if (node["loop"])
            r.loop = node["loop"].as<bool>();
        outConfig = r;
        return true;
    }
    }
    return false;
}

}  // namespace

bool ConnectionManager::loadConfigFile(const QString& path) {
    // Reset to empty before loading.
    while (!orderedIds_.isEmpty()) {
        const QString id = orderedIds_.first();
        connections_.erase(id);
        orderedIds_.removeFirst();
    }

    std::ifstream in(path.toStdString());
    if (!in.is_open()) {
        SF_LOG_INFO("ConnectionManager::loadConfigFile: file not found at {}", path.toStdString());
        return false;
    }
    std::stringstream contents;
    contents << in.rdbuf();

    YAML::Node doc;
    try {
        doc = YAML::Load(contents.str());
    } catch (const YAML::Exception& e) {
        SF_LOG_ERROR("ConnectionManager: yaml parse error in {}: {}", path.toStdString(), e.what());
        return false;
    }

    if (!doc.IsMap()) {
        SF_LOG_ERROR("ConnectionManager: yaml top level is not a map in {}", path.toStdString());
        return false;
    }
    if (!doc["schema_version"]) {
        SF_LOG_ERROR("ConnectionManager: missing schema_version in {}", path.toStdString());
        return false;
    }
    const int version = doc["schema_version"].as<int>(-1);
    if (version != kSchemaVersion) {
        SF_LOG_ERROR("ConnectionManager: unsupported schema_version {} in {} (expected {})", version,
                     path.toStdString(), kSchemaVersion);
        return false;
    }
    if (!doc["connections"] || !doc["connections"].IsSequence()) {
        SF_LOG_ERROR("ConnectionManager: missing or non-sequence 'connections' in {}", path.toStdString());
        return false;
    }

    for (const auto& node : doc["connections"]) {
        if (!node.IsMap() || !node["id"] || !node["driverType"] || !node["driverConfig"]) {
            SF_LOG_WARN("ConnectionManager: skipping malformed connection entry in {}", path.toStdString());
            continue;
        }
        ConnectionConfig cfg;
        cfg.id = QString::fromStdString(node["id"].as<std::string>());
        if (node["displayName"]) {
            cfg.displayName = QString::fromStdString(node["displayName"].as<std::string>());
        }
        const auto type = driverTypeFromYaml(node["driverType"].as<std::string>());
        if (!type) {
            SF_LOG_WARN("ConnectionManager: unknown driverType '{}' for id '{}'", node["driverType"].as<std::string>(),
                        cfg.id.toStdString());
            continue;
        }
        cfg.driverType = *type;
        if (!readDriverConfig(node["driverConfig"], cfg.driverType, cfg.driverConfig)) {
            SF_LOG_WARN("ConnectionManager: invalid driverConfig for id '{}'", cfg.id.toStdString());
            continue;
        }
        if (node["decoderSchemaId"]) {
            cfg.decoderSchemaId = QString::fromStdString(node["decoderSchemaId"].as<std::string>());
        }
        if (node["autoConnectOnStartup"]) {
            cfg.autoConnectOnStartup = node["autoConnectOnStartup"].as<bool>();
        }
        if (node["autoConnectCommands"] && node["autoConnectCommands"].IsSequence()) {
            for (const auto& cmdNode : node["autoConnectCommands"]) {
                if (!cmdNode.IsMap() || !cmdNode["payload"]) {
                    continue;
                }
                AutoConnectCommand cmd;
                if (cmdNode["name"]) {
                    cmd.name = QString::fromStdString(cmdNode["name"].as<std::string>());
                }
                cmd.payload = fromBinary(cmdNode["payload"].as<YAML::Binary>());
                if (cmdNode["expected"]) {
                    cmd.expected = fromBinary(cmdNode["expected"].as<YAML::Binary>());
                }
                if (cmdNode["timeout"]) {
                    cmd.timeout = std::chrono::milliseconds{cmdNode["timeout"].as<long long>()};
                }
                if (cmdNode["delayBefore"]) {
                    cmd.delayBefore = std::chrono::milliseconds{cmdNode["delayBefore"].as<long long>()};
                }
                cfg.autoConnectCommands.push_back(std::move(cmd));
            }
        }

        // Build the Connection without going through addConnection
        // (which would auto-save and emit add signal mid-load).
        if (cfg.id.isEmpty() || connections_.find(cfg.id) != connections_.end()) {
            SF_LOG_WARN("ConnectionManager: skipping connection with empty/duplicate id");
            continue;
        }
        auto conn = std::make_unique<Connection>(cfg, this);
        Connection& ref = *conn;
        const QString id = cfg.id;
        insertConnection(id, std::move(conn));
        wireConnection(id, ref);
        // ADR-008: each loaded connection wires its decoderSchemaId
        // into the DecoderRegistrar's runtime map. For multi-conn-
        // same-type, last-loaded-wins (per ADR known limitation).
        applyDecoderSchemaForConfig(decoderRegistrar_, cfg);
        Q_EMIT connectionAdded(id);
    }

    configPath_ = path;
    return true;
}

bool ConnectionManager::saveConfigFile(const QString& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "schema_version" << YAML::Value << kSchemaVersion;
    out << YAML::Key << "connections" << YAML::Value << YAML::BeginSeq;
    for (const QString& id : orderedIds_) {
        auto it = connections_.find(id);
        if (it == connections_.end() || !it->second) {
            continue;
        }
        const auto& cfg = it->second->config();
        out << YAML::BeginMap;
        out << YAML::Key << "id" << YAML::Value << cfg.id.toStdString();
        out << YAML::Key << "displayName" << YAML::Value << cfg.displayName.toStdString();
        out << YAML::Key << "driverType" << YAML::Value << driverTypeToYaml(cfg.driverType);
        out << YAML::Key << "driverConfig" << YAML::Value;
        emitDriverConfig(out, cfg);
        out << YAML::Key << "decoderSchemaId" << YAML::Value << cfg.decoderSchemaId.toStdString();
        out << YAML::Key << "autoConnectOnStartup" << YAML::Value << cfg.autoConnectOnStartup;
        out << YAML::Key << "autoConnectCommands" << YAML::Value << YAML::BeginSeq;
        for (const auto& cmd : cfg.autoConnectCommands) {
            out << YAML::BeginMap;
            out << YAML::Key << "name" << YAML::Value << cmd.name.toStdString();
            out << YAML::Key << "payload" << YAML::Value << toBinary(cmd.payload);
            if (cmd.expected.has_value()) {
                out << YAML::Key << "expected" << YAML::Value << toBinary(*cmd.expected);
            }
            out << YAML::Key << "timeout" << YAML::Value << static_cast<long long>(cmd.timeout.count());
            out << YAML::Key << "delayBefore" << YAML::Value << static_cast<long long>(cmd.delayBefore.count());
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        SF_LOG_ERROR("ConnectionManager::saveConfigFile: cannot open {} for write", path.toStdString());
        return false;
    }
    f.write(out.c_str(), out.size());
    f.write("\n", 1);
    f.close();
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
