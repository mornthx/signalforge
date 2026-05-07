// src/chart/chart_manager.cpp
//
// S5 — create / remove / lookup + activeChartId tracking.
// S7 — yaml save/load via yaml-cpp. Schema documented in
//      `schemas/charts_v1.yaml` (canonical example).

#include "chart/chart_manager.hpp"

#include "observability/logging.hpp"

#include <QFile>
#include <QFileInfo>
#include <fstream>
#include <sstream>
#include <yaml-cpp/yaml.h>

namespace signalforge::chart {

namespace {

[[nodiscard]] const char* displayModeToYaml(SignalDisplayMode mode) noexcept {
    switch (mode) {
    case SignalDisplayMode::Line:
        return "line";
    case SignalDisplayMode::Step:
        return "step";
    case SignalDisplayMode::Point:
        return "point";
    }
    return "line";
}

[[nodiscard]] SignalDisplayMode displayModeFromYaml(const std::string& s) noexcept {
    if (s == "step") {
        return SignalDisplayMode::Step;
    }
    if (s == "point") {
        return SignalDisplayMode::Point;
    }
    return SignalDisplayMode::Line;
}

}  // namespace

ChartManager::ChartManager(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry), timeAxis_(std::make_unique<TimeAxisManager>(this)) {}

ChartManager::~ChartManager() = default;

QString ChartManager::createChart(const ChartConfig& config) {
    // Honor a non-empty `config.id` if it doesn't collide; otherwise
    // generate `chart-<n>` with a monotonic suffix.
    QString id = config.id;
    if (id.isEmpty() || charts_.contains(id)) {
        do {
            id = QStringLiteral("chart-%1").arg(nextChartIdSuffix_++);
        } while (charts_.contains(id));
    }
    auto chart = std::make_unique<Chart>(*registry_, *timeAxis_, config);
    auto* raw = chart.get();
    raw->setObjectName(id);

    ChartConfig cfg = config;
    cfg.id = id;
    raw->setConfig(std::move(cfg));

    charts_[id] = std::move(chart);
    chartOrder_.append(id);
    if (activeChartId_.isEmpty()) {
        activeChartId_ = id;
        Q_EMIT activeChartChanged(id);
    }
    Q_EMIT chartCreated(id);
    return id;
}

bool ChartManager::removeChart(const QString& chartId) {
    auto it = charts_.find(chartId);
    if (it == charts_.end()) {
        return false;
    }
    charts_.erase(it);
    chartOrder_.removeAll(chartId);
    if (activeChartId_ == chartId) {
        activeChartId_ = chartOrder_.isEmpty() ? QString{} : chartOrder_.first();
        Q_EMIT activeChartChanged(activeChartId_);
    }
    Q_EMIT chartRemoved(chartId);
    return true;
}

Chart* ChartManager::chart(const QString& chartId) const {
    auto it = charts_.find(chartId);
    return it == charts_.end() ? nullptr : it->second.get();
}

QStringList ChartManager::chartIds() const {
    return chartOrder_;
}

QString ChartManager::activeChartId() const {
    return activeChartId_;
}

void ChartManager::setActiveChartId(const QString& chartId) {
    if (chartId == activeChartId_) {
        return;
    }
    if (!charts_.contains(chartId)) {
        return;  // Defensive: ignore unknown ids.
    }
    activeChartId_ = chartId;
    Q_EMIT activeChartChanged(chartId);
}

TimeAxisManager& ChartManager::timeAxis() {
    return *timeAxis_;
}

void ChartManager::saveConfigToFile(const QString& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "schema_version" << YAML::Value << 1;
    out << YAML::Key << "charts" << YAML::Value << YAML::BeginSeq;
    for (const auto& chartId : chartOrder_) {
        const auto cfg = charts_.at(chartId)->config();
        out << YAML::BeginMap;
        out << YAML::Key << "id" << YAML::Value << cfg.id.toStdString();
        out << YAML::Key << "title" << YAML::Value << cfg.title.toStdString();
        if (cfg.timeAxisId.has_value()) {
            out << YAML::Key << "time_axis_id" << YAML::Value << cfg.timeAxisId->toStdString();
        }
        out << YAML::Key << "signals" << YAML::Value << YAML::BeginSeq;
        for (const auto& s : cfg.signalConfigs) {
            out << YAML::BeginMap;
            out << YAML::Key << "signal_id" << YAML::Value << s.signalId.toStdString();
            out << YAML::Key << "display_mode" << YAML::Value << displayModeToYaml(s.displayMode);
            if (s.color.isValid()) {
                out << YAML::Key << "color" << YAML::Value << s.color.name().toStdString();
            }
            out << YAML::Key << "visible" << YAML::Value << s.visible;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        SF_LOG_ERROR("ChartManager: cannot open {} for write: {}", path.toStdString(), f.errorString().toStdString());
        return;
    }
    f.write(out.c_str());
}

bool ChartManager::loadConfigFromFile(const QString& path) {
    if (!QFileInfo::exists(path)) {
        // Missing file is a graceful degradation: leave the manager
        // empty / unchanged, return false. Spec §8.4.
        return false;
    }
    YAML::Node doc;
    try {
        std::ifstream stream(path.toStdString());
        if (!stream.is_open()) {
            return false;
        }
        std::stringstream contents;
        contents << stream.rdbuf();
        doc = YAML::Load(contents.str());
    } catch (const YAML::Exception& e) {
        SF_LOG_ERROR("ChartManager: yaml parse error in {}: {}", path.toStdString(), e.what());
        return false;
    }
    if (!doc.IsMap() || !doc["charts"] || !doc["charts"].IsSequence()) {
        SF_LOG_ERROR("ChartManager: yaml missing top-level 'charts' sequence in {}", path.toStdString());
        return false;
    }

    // Replace existing manager state. Charts that were active are
    // dropped; the active id rotates via createChart().
    while (!chartOrder_.isEmpty()) {
        removeChart(chartOrder_.first());
    }

    for (const auto& chartNode : doc["charts"]) {
        if (!chartNode.IsMap()) {
            continue;
        }
        ChartConfig cfg;
        cfg.id = QString::fromStdString(chartNode["id"].as<std::string>(""));
        cfg.title = QString::fromStdString(chartNode["title"].as<std::string>(""));
        if (chartNode["time_axis_id"]) {
            cfg.timeAxisId = QString::fromStdString(chartNode["time_axis_id"].as<std::string>(""));
        }
        if (chartNode["signals"] && chartNode["signals"].IsSequence()) {
            for (const auto& sigNode : chartNode["signals"]) {
                if (!sigNode.IsMap()) {
                    continue;
                }
                ChartSignalConfig s;
                s.signalId = QString::fromStdString(sigNode["signal_id"].as<std::string>(""));
                s.displayMode = displayModeFromYaml(sigNode["display_mode"].as<std::string>("line"));
                if (sigNode["color"]) {
                    s.color = QColor(QString::fromStdString(sigNode["color"].as<std::string>("")));
                }
                s.visible = sigNode["visible"].as<bool>(true);
                cfg.signalConfigs.push_back(std::move(s));
            }
        }
        (void)createChart(cfg);  // [[nodiscard]] return is not needed here
    }
    return true;
}

}  // namespace signalforge::chart
