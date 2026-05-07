// src/chart/signal_selector.cpp
//
// S1 ctor/dtor stubs. Full implementation lands in S6 (tree
// building, registry observation, filter, checkbox handling).

#include "chart/signal_selector.hpp"

namespace signalforge::chart {

struct SignalSelector::Impl {
    QString filter;
    // S6 will hold the QTreeWidget, line-edit, and per-driver
    // top-level items here.
};

SignalSelector::SignalSelector(signalforge::buffer::SignalBufferRegistry& registry, ChartManager& manager,
                               QWidget* parent)
    : QWidget(parent), registry_(&registry), manager_(&manager), impl_(std::make_unique<Impl>()) {}

SignalSelector::~SignalSelector() = default;

void SignalSelector::setFilter(const QString& substring) {
    impl_->filter = substring;
}

QString SignalSelector::filter() const {
    return impl_->filter;
}

}  // namespace signalforge::chart
