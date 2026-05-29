// src/dashboard/plot_panel.cpp

#include "dashboard/plot_panel.hpp"

#include "chart/chart.hpp"
#include "observability/logging.hpp"

#include <QQuickItem>
#include <QQuickWidget>
#include <QUrl>

namespace signalforge::dashboard {

PlotPanel::PlotPanel(PanelConfig config, signalforge::chart::Chart* chart, QWidget* parent)
    : Panel(std::move(config), parent), chart_(chart) {
    host_ = new QQuickWidget(this);
    host_->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // QQuickWidget's sizeHint is invalid until the scene loads; force
    // Expanding so the host fills the panel body (ADR-011 rationale).
    host_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    host_->setMinimumSize(1, 1);
    host_->setSource(QUrl(QStringLiteral("qrc:/qml/ChartHost.qml")));

    setBody(host_);

    if (host_->status() != QQuickWidget::Ready) {
        SF_LOG_ERROR("PlotPanel: ChartHost.qml failed to load (status={})", static_cast<int>(host_->status()));
        return;
    }
    auto* root = host_->rootObject();
    if (root == nullptr) {
        SF_LOG_ERROR("PlotPanel: ChartHost.qml loaded but rootObject() is null");
        return;
    }
    if (chart_ != nullptr) {
        chart_->setParentItem(root);
        // ADR-011: bind the chart's geometry to the host scene root.
        const auto syncSize = [chart = chart_, root]() { chart->setSize(QSizeF(root->width(), root->height())); };
        syncSize();
        connect(root, &QQuickItem::widthChanged, chart_, syncSize);
        connect(root, &QQuickItem::heightChanged, chart_, syncSize);

        // Reflect any pre-configured signals into both the chart and the
        // panel binding so config()/hasSignal() stay consistent.
        const QStringList preconfigured = config_.signalIds;
        config_.signalIds.clear();
        for (const QString& id : preconfigured) {
            addSignal(id);
        }
    }
}

PlotPanel::~PlotPanel() {
    // Detach the manager-owned chart before our QQuickWidget is
    // destroyed, so the widget never deletes a chart it doesn't own.
    if (chart_ != nullptr) {
        chart_->setParentItem(nullptr);
    }
}

void PlotPanel::detachChart() {
    if (chart_ != nullptr) {
        chart_->setParentItem(nullptr);
        chart_ = nullptr;
    }
}

void PlotPanel::addSignal(const QString& signalId) {
    if (signalId.isEmpty() || config_.signalIds.contains(signalId)) {
        return;
    }
    config_.signalIds.append(signalId);
    if (chart_ != nullptr) {
        chart_->addSignal(signalId);
    }
}

void PlotPanel::removeSignal(const QString& signalId) {
    config_.signalIds.removeAll(signalId);
    if (chart_ != nullptr) {
        chart_->removeSignal(signalId);
    }
}

}  // namespace signalforge::dashboard
