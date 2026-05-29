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

#include <QAction>
#include <QCursor>
#include <QGridLayout>
#include <QLayoutItem>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <algorithm>

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

Panel* Dashboard::makePanel(const PanelConfig& config) {
    switch (config.type) {
    case PanelType::Numeric:
        return new NumericPanel(config, *registry_, this);
    case PanelType::State:
        return new StatePanel(config, *registry_, this);
    case PanelType::Table:
        return new TablePanel(config, *registry_, this);
    case PanelType::Plot:
        return new PlotPanel(config, *registry_, *timeAxis_, this);
    case PanelType::Bar:
    case PanelType::Gauge:
        return new MeterPanel(config, *registry_, this);
    }
    return new NumericPanel(config, *registry_, this);
}

QString Dashboard::addPanel(PanelConfig config) {
    if (config.id.isEmpty() || panels_.contains(config.id)) {
        config.id = nextPanelId();
    }
    const QString id = config.id;

    Panel* created = makePanel(config);
    connect(created, &Panel::configureRequested, this, &Dashboard::showPanelMenu);
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

namespace {
bool isSingleSignalType(PanelType t) {
    return t == PanelType::Numeric || t == PanelType::State || t == PanelType::Bar || t == PanelType::Gauge;
}
}  // namespace

void Dashboard::recreatePanel(const QString& panelId, PanelConfig newConfig) {
    const int idx = panelOrder_.indexOf(panelId);
    if (idx < 0) {
        return;
    }
    newConfig.id = panelId;
    Panel* old = panels_.value(panelId);
    Panel* fresh = makePanel(newConfig);
    connect(fresh, &Panel::configureRequested, this, &Dashboard::showPanelMenu);
    panels_.insert(panelId, fresh);  // replace in the hash
    if (old != nullptr) {
        old->deleteLater();
    }
    relayout();  // panelOrder_ position is unchanged (same id at same index)
    Q_EMIT panelsChanged();
}

void Dashboard::setPanelType(const QString& panelId, PanelType type) {
    Panel* p = panels_.value(panelId, nullptr);
    if (p == nullptr || p->type() == type) {
        return;
    }
    PanelConfig cfg = p->config();
    cfg.type = type;
    // Single-signal widgets keep at most one signal.
    if (isSingleSignalType(type) && cfg.signalIds.size() > 1) {
        cfg.signalIds = {cfg.signalIds.first()};
    }
    recreatePanel(panelId, std::move(cfg));
}

void Dashboard::setPanelSignals(const QString& panelId, const QStringList& signalIds) {
    Panel* p = panels_.value(panelId, nullptr);
    if (p == nullptr) {
        return;
    }
    PanelConfig cfg = p->config();
    cfg.signalIds = signalIds;
    if (isSingleSignalType(cfg.type) && cfg.signalIds.size() > 1) {
        cfg.signalIds = {cfg.signalIds.first()};
    }
    recreatePanel(panelId, std::move(cfg));
}

void Dashboard::movePanel(const QString& panelId, int delta) {
    const int idx = panelOrder_.indexOf(panelId);
    if (idx < 0) {
        return;
    }
    const int target = std::clamp(idx + delta, 0, static_cast<int>(panelOrder_.size()) - 1);
    if (target == idx) {
        return;
    }
    panelOrder_.move(idx, target);
    relayout();
}

QMenu* Dashboard::buildPanelMenu(const QString& panelId) {
    auto* menu = new QMenu(this);
    Panel* p = panels_.value(panelId, nullptr);
    if (p == nullptr) {
        return menu;
    }
    const PanelType cur = p->type();

    auto* showAs = menu->addMenu(tr("Show as"));
    struct TypeEntry {
        PanelType type;
        QString label;
    };
    const TypeEntry entries[] = {{PanelType::Numeric, tr("Numeric")}, {PanelType::State, tr("State")},
                                 {PanelType::Plot, tr("Plot")},       {PanelType::Bar, tr("Bar")},
                                 {PanelType::Gauge, tr("Gauge")},     {PanelType::Table, tr("Table")}};
    for (const auto& e : entries) {
        QAction* a = showAs->addAction(e.label);
        a->setCheckable(true);
        a->setChecked(e.type == cur);
        const PanelType t = e.type;
        connect(a, &QAction::triggered, this, [this, panelId, t]() { setPanelType(panelId, t); });
    }

    auto* sigMenu = menu->addMenu(tr("Signals"));
    const QStringList all = registry_->signalIds();
    if (all.isEmpty()) {
        QAction* none = sigMenu->addAction(tr("(no signals available)"));
        none->setEnabled(false);
    }
    const bool single = isSingleSignalType(cur);
    for (const QString& sid : all) {
        QAction* a = sigMenu->addAction(sid);
        a->setCheckable(true);
        a->setChecked(p->hasSignal(sid));
        connect(a, &QAction::triggered, this, [this, panelId, sid, single]() {
            Panel* pp = panels_.value(panelId, nullptr);
            if (pp == nullptr) {
                return;
            }
            QStringList sigs = pp->signalIds();
            if (single) {
                sigs = {sid};
            } else if (sigs.contains(sid)) {
                sigs.removeAll(sid);
            } else {
                sigs.append(sid);
            }
            setPanelSignals(panelId, sigs);
        });
    }

    menu->addSeparator();
    connect(menu->addAction(tr("Move ◀ left")), &QAction::triggered, this,
            [this, panelId]() { movePanel(panelId, -1); });
    connect(menu->addAction(tr("Move right ▶")), &QAction::triggered, this,
            [this, panelId]() { movePanel(panelId, 1); });
    menu->addSeparator();
    connect(menu->addAction(tr("Remove panel")), &QAction::triggered, this,
            [this, panelId]() { removePanel(panelId); });
    return menu;
}

void Dashboard::showPanelMenu(const QString& panelId) {
    Panel* p = panels_.value(panelId, nullptr);
    if (p == nullptr) {
        return;
    }
    QMenu* menu = buildPanelMenu(panelId);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* btn = p->configButton();
    const QPoint pos = (btn != nullptr) ? btn->mapToGlobal(QPoint(0, btn->height())) : QCursor::pos();
    menu->popup(pos);
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
