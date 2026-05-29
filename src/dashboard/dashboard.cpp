// src/dashboard/dashboard.cpp

#include "dashboard/dashboard.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "dashboard/meter_panel.hpp"
#include "dashboard/numeric_panel.hpp"
#include "dashboard/panel.hpp"
#include "dashboard/panel_factory.hpp"
#include "dashboard/plot_panel.hpp"
#include "dashboard/state_panel.hpp"
#include "dashboard/table_panel.hpp"
#include "decode/decoder_interface.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QTimer>

namespace signalforge::dashboard {

namespace {
constexpr int kRefreshIntervalMs = 66;  ///< ~15 Hz; Numeric/State cards.
}

Dashboard::Dashboard(signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : QWidget(parent), registry_(&registry), timeAxis_(std::make_unique<signalforge::chart::TimeAxisManager>()) {
    setObjectName(QStringLiteral("dashboardSurface"));
    grid_ = new QGridLayout(this);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(2);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &Dashboard::refreshAll);
    refreshTimer_->start();
}

Dashboard::~Dashboard() = default;

QString Dashboard::nextPanelId() {
    QString id;
    do {
        id = QStringLiteral("panel-%1").arg(nextPanelSuffix_++);
    } while (panels_.contains(id));
    return id;
}

Panel* Dashboard::panel(const QString& panelId) const {
    return panels_.value(panelId, nullptr);
}

bool Dashboard::showsSignal(const QString& signalId) const {
    for (const QString& id : panelOrder_) {
        if (panels_.value(id)->hasSignal(signalId)) {
            return true;
        }
    }
    return false;
}

QString Dashboard::addPanel(PanelConfig config) {
    if (config.id.isEmpty() || panels_.contains(config.id)) {
        config.id = nextPanelId();
    }
    const QString id = config.id;

    Panel* created = nullptr;
    switch (config.type) {
    case PanelType::Numeric:
        created = new NumericPanel(config, *registry_, this);
        break;
    case PanelType::State:
        created = new StatePanel(config, *registry_, this);
        break;
    case PanelType::Table:
        created = new TablePanel(config, *registry_, this);
        break;
    case PanelType::Plot:
        created = new PlotPanel(config, *registry_, *timeAxis_, this);
        break;
    case PanelType::Bar:
    case PanelType::Gauge:
        created = new MeterPanel(config, *registry_, this);
        break;
    }

    created->setEditMode(editMode_);
    connect(created, &Panel::removeRequested, this, &Dashboard::removePanel);
    panels_.insert(id, created);
    panelOrder_.append(id);
    relayout();
    Q_EMIT panelsChanged();
    return id;
}

QString Dashboard::addSignal(const QString& signalId) {
    if (signalId.isEmpty()) {
        return {};
    }
    // Already shown somewhere?
    for (const QString& id : panelOrder_) {
        if (panels_.value(id)->hasSignal(signalId)) {
            return id;
        }
    }
    PanelType type = PanelType::Numeric;
    if (auto* buf = registry_->bufferFor(signalId); buf != nullptr) {
        type = suggestPanelType(buf->metadata().type);
    }
    PanelConfig cfg;
    cfg.type = type;
    cfg.signalIds << signalId;
    return addPanel(std::move(cfg));
}

QString Dashboard::addPlotPanel(const QStringList& signalIds) {
    PanelConfig cfg;
    cfg.type = PanelType::Plot;
    cfg.signalIds = signalIds;
    return addPanel(std::move(cfg));
}

QString Dashboard::addTablePanel(const QStringList& signalIds) {
    PanelConfig cfg;
    cfg.type = PanelType::Table;
    cfg.signalIds = signalIds;
    return addPanel(std::move(cfg));
}

QString Dashboard::addBarPanel(const QString& signalId) {
    PanelConfig cfg;
    cfg.type = PanelType::Bar;
    if (!signalId.isEmpty()) {
        cfg.signalIds << signalId;
    }
    return addPanel(std::move(cfg));
}

QString Dashboard::addGaugePanel(const QString& signalId) {
    PanelConfig cfg;
    cfg.type = PanelType::Gauge;
    if (!signalId.isEmpty()) {
        cfg.signalIds << signalId;
    }
    return addPanel(std::move(cfg));
}

void Dashboard::removeSignalEverywhere(const QString& signalId) {
    // Copy the order: removePanel mutates panelOrder_.
    const QStringList order = panelOrder_;
    for (const QString& id : order) {
        Panel* p = panels_.value(id, nullptr);
        if (p == nullptr || !p->hasSignal(signalId)) {
            continue;
        }
        if (p->isMultiSignal()) {
            // Plot / Table: drop just this signal's trace/row, keep the panel.
            p->removeSignal(signalId);
        } else {
            // Single-signal card: removing its signal removes the card.
            removePanel(id);
        }
    }
}

void Dashboard::removePanel(const QString& panelId) {
    auto it = panels_.find(panelId);
    if (it == panels_.end()) {
        return;
    }
    Panel* p = it.value();
    panels_.erase(it);
    panelOrder_.removeAll(panelId);
    p->deleteLater();
    relayout();
    Q_EMIT panelsChanged();
}

void Dashboard::setEditMode(bool on) {
    editMode_ = on;
    for (const QString& id : panelOrder_) {
        panels_.value(id)->setEditMode(on);
    }
}

signalforge::chart::TimeAxisManager& Dashboard::timeAxis() {
    return *timeAxis_;
}

void Dashboard::refreshAll() {
    for (const QString& id : panelOrder_) {
        panels_.value(id)->refresh();
    }
}

void Dashboard::relayout() {
    // Detach every panel from the grid without deleting it.
    while (QLayoutItem* item = grid_->takeAt(0)) {
        delete item;  // frees the layout item, not the widget.
    }
    // Reset row stretches from the previous layout (QGridLayout keeps them).
    for (int r = 0; r < 64; ++r) {
        grid_->setRowStretch(r, 0);
    }

    int row = 0;
    int col = 0;
    bool anyWide = false;
    for (const QString& id : panelOrder_) {
        Panel* p = panels_.value(id);
        if (p->isWide()) {
            if (col != 0) {
                ++row;
                col = 0;
            }
            grid_->addWidget(p, row, 0, 1, columns_);
            grid_->setRowStretch(row, 1);  // plots/tables fill vertical space
            anyWide = true;
            ++row;
        } else {
            grid_->addWidget(p, row, col, 1, 1);
            if (++col >= columns_) {
                ++row;
                col = 0;
            }
        }
    }
    // With only small cards, a trailing row absorbs slack so they pack top.
    if (!anyWide) {
        grid_->setRowStretch(row + 1, 1);
    }
}

}  // namespace signalforge::dashboard
