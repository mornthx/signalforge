// src/chart/signal_selector.cpp
//
// S6 — QTreeWidget-based signal selector. Groups by driver id
// derived from the M5 signal-id convention (`<driverId>/<fieldName>`);
// signals without a `/` are M7 derived signals (expression-engine
// virtual driver) and land in the "Derived" group.
//
// The M6 SignalBufferRegistry is not a QObject (M6 freeze) and
// cannot emit Qt signals; SignalSelector exposes a `refresh()`
// slot that callers (S8 MainWindow) invoke after registry mutation.
// See `.claude/M8-concerns.md` C2.

#include "chart/signal_selector.hpp"

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <unordered_map>

namespace signalforge::chart {

namespace {

constexpr auto kDerivedDriverId = "expression-engine";  ///< M7 virtual driver id.
constexpr auto kDerivedGroupLabel = "Derived";

/// Split a signal id at the first `/`. Everything before is the
/// driver id; everything after is the signal name.
[[nodiscard]] std::pair<QString, QString> splitSignalId(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return {QString{}, signalId};
    }
    return {signalId.left(slash), signalId.mid(slash + 1)};
}

/// Tree key for a signal id: "Derived" for derived signals,
/// "Driver: <driverId>" otherwise.
[[nodiscard]] QString groupLabelForSignal(const QString& signalId) {
    const auto parts = splitSignalId(signalId);
    if (parts.first.isEmpty() || parts.first == QLatin1String(kDerivedDriverId)) {
        return QString::fromUtf8(kDerivedGroupLabel);
    }
    return QStringLiteral("Driver: %1").arg(parts.first);
}

}  // namespace

struct SignalSelector::Impl {
    QString filter;
    QLineEdit* filterEdit = nullptr;
    QTreeWidget* tree = nullptr;
    /// Group label → top-level QTreeWidgetItem*.
    std::unordered_map<QString, QTreeWidgetItem*> groups;
    /// Signal id → leaf QTreeWidgetItem*.
    std::unordered_map<QString, QTreeWidgetItem*> leaves;
};

SignalSelector::SignalSelector(signalforge::buffer::SignalBufferRegistry& registry, ChartManager& manager,
                               QWidget* parent)
    : QWidget(parent), registry_(&registry), manager_(&manager), impl_(std::make_unique<Impl>()) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    impl_->filterEdit = new QLineEdit(this);
    impl_->filterEdit->setPlaceholderText(tr("Filter signals…"));
    layout->addWidget(impl_->filterEdit);

    impl_->tree = new QTreeWidget(this);
    impl_->tree->setHeaderLabel(tr("Signals"));
    impl_->tree->header()->setStretchLastSection(true);
    layout->addWidget(impl_->tree);

    QObject::connect(impl_->filterEdit, &QLineEdit::textChanged, this,
                     [this](const QString& text) { setFilter(text); });

    QObject::connect(impl_->tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column != 0 || item == nullptr) {
            return;
        }
        // Only react to leaves (those with a stored
        // signal id in UserRole). Top-level driver
        // groups don't have one.
        const QString signalId = item->data(0, Qt::UserRole).toString();
        if (signalId.isEmpty()) {
            return;
        }
        const bool checked = item->checkState(0) == Qt::Checked;
        Q_EMIT signalToggled(signalId, checked);
        // Default behavior: route to the active chart.
        if (auto* active = manager_->chart(manager_->activeChartId())) {
            if (checked) {
                active->addSignal(signalId);
            } else {
                active->removeSignal(signalId);
            }
        }
    });

    refresh();
}

SignalSelector::~SignalSelector() = default;

void SignalSelector::setFilter(const QString& substring) {
    impl_->filter = substring;
    // Apply: hide leaves whose signal id doesn't contain the
    // substring (case-insensitive); hide a group if all its
    // children are hidden.
    for (auto& [signalId, leaf] : impl_->leaves) {
        const bool match = substring.isEmpty() || signalId.contains(substring, Qt::CaseInsensitive);
        leaf->setHidden(!match);
    }
    for (auto& [label, group] : impl_->groups) {
        bool anyVisible = false;
        for (int i = 0; i < group->childCount(); ++i) {
            if (!group->child(i)->isHidden()) {
                anyVisible = true;
                break;
            }
        }
        group->setHidden(!anyVisible);
    }
}

QString SignalSelector::filter() const {
    return impl_->filter;
}

void SignalSelector::refresh() {
    impl_->tree->clear();
    impl_->groups.clear();
    impl_->leaves.clear();

    // Block itemChanged emissions during programmatic population
    // so we don't accidentally toggle signals into the active
    // chart from the rebuild path.
    const QSignalBlocker blocker(impl_->tree);

    const auto signalIds = registry_->signalIds();
    for (const auto& signalId : signalIds) {
        auto* buf = registry_->bufferFor(signalId);
        if (buf == nullptr) {
            continue;
        }
        const auto& meta = buf->metadata();
        const QString groupLabel = groupLabelForSignal(signalId);

        QTreeWidgetItem* group = nullptr;
        if (auto it = impl_->groups.find(groupLabel); it != impl_->groups.end()) {
            group = it->second;
        } else {
            group = new QTreeWidgetItem(impl_->tree);
            group->setText(0, groupLabel);
            group->setExpanded(true);
            impl_->groups[groupLabel] = group;
        }

        // Leaf label: "<signalName>  (<unit>)" if unit is set, else
        // just the signal name. Field portion of the signal id is
        // shown when no separate `name` is configured.
        const QString fieldPart = splitSignalId(signalId).second;
        QString leafText = meta.name.isEmpty() ? fieldPart : meta.name;
        if (!meta.unit.isEmpty()) {
            leafText += QStringLiteral("  (") + meta.unit + QStringLiteral(")");
        }

        auto* leaf = new QTreeWidgetItem(group);
        leaf->setText(0, leafText);
        leaf->setData(0, Qt::UserRole, signalId);
        leaf->setFlags(leaf->flags() | Qt::ItemIsUserCheckable);

        // Reflect current "is in active chart?" as initial check.
        Qt::CheckState initial = Qt::Unchecked;
        if (auto* active = manager_->chart(manager_->activeChartId())) {
            if (active->visibleSignals().contains(signalId)) {
                initial = Qt::Checked;
            }
        }
        leaf->setCheckState(0, initial);

        impl_->leaves[signalId] = leaf;
    }

    // Re-apply the existing filter so newly-added items respect it.
    setFilter(impl_->filter);
}

}  // namespace signalforge::chart
