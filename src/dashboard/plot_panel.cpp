// src/dashboard/plot_panel.cpp

#include "dashboard/plot_panel.hpp"

#include "dashboard/plot_view.hpp"

#include <utility>

namespace signalforge::dashboard {

PlotPanel::PlotPanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry,
                     signalforge::chart::TimeAxisManager& timeAxis, QWidget* parent)
    : Panel(std::move(config), parent) {
    view_ = new PlotView(registry, timeAxis, this);
    setBody(view_);
    if (config_.title.isEmpty()) {
        setHeaderTitle(tr("Plot"));
    }

    // Reflect any pre-configured signals into both the view and the panel
    // binding so config()/hasSignal() stay consistent.
    const QStringList preconfigured = config_.signalIds;
    config_.signalIds.clear();
    for (const QString& id : preconfigured) {
        addSignal(id);
    }

    // Apply the configured Y-range (nullopt → auto-scale). Editing a plot's
    // range goes through applyPanelConfig, which re-creates the panel, so the
    // range is (re)applied here on construction. (Not in refresh(): setYRange
    // itself refreshes the view, which would double-refresh every tick.)
    view_->setYRange(config_.rangeMin, config_.rangeMax);
}

PlotPanel::~PlotPanel() = default;

void PlotPanel::addSignal(const QString& signalId) {
    if (signalId.isEmpty() || config_.signalIds.contains(signalId)) {
        return;
    }
    config_.signalIds.append(signalId);
    if (view_ != nullptr) {
        view_->addSignal(signalId);
    }
}

void PlotPanel::removeSignal(const QString& signalId) {
    config_.signalIds.removeAll(signalId);
    if (view_ != nullptr) {
        view_->removeSignal(signalId);
    }
}

void PlotPanel::refresh() {
    if (view_ != nullptr) {
        view_->refresh();
    }
}

void PlotPanel::setSignalColorProvider(SignalColorProvider provider) {
    if (view_ != nullptr) {
        view_->setColorProvider(std::move(provider));
    }
}

}  // namespace signalforge::dashboard
