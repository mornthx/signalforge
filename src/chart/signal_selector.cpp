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
//
// M17 S3 — added panelHeader title row + filter-count caption +
// switched `groups` / `leaves` from `std::unordered_map` to
// `std::map` so tree population is deterministic per ADR-014's
// consumer-side closure. See
// `docs/v0.3/widget-styling-guide.md` §3 / §4 / §9 row 6.

#include "chart/signal_selector.hpp"

#include "buffer/signal_buffer.hpp"
#include "decode/decoder_interface.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <map>

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
    QFrame* header = nullptr;
    QLabel* headerLabel = nullptr;
    QLineEdit* filterEdit = nullptr;
    QLabel* countLabel = nullptr;
    QTreeWidget* tree = nullptr;
    /// Group label → top-level QTreeWidgetItem*. std::map for
    /// alphabetical (deterministic) iteration order (M17 S3,
    /// ADR-014 anti-pattern row 6).
    std::map<QString, QTreeWidgetItem*> groups;
    /// Signal id → leaf QTreeWidgetItem*. std::map for the same
    /// reason as `groups`.
    std::map<QString, QTreeWidgetItem*> leaves;
};

SignalSelector::SignalSelector(signalforge::buffer::SignalBufferRegistry& registry, ChartManager& manager,
                               QWidget* parent)
    : QWidget(parent), registry_(&registry), manager_(&manager), impl_(std::make_unique<Impl>()) {
    // M17 S4 — stable objectName for visual-test + AT-SPI tooling.
    setObjectName(QStringLiteral("signalSelectorPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // M17 S3 — panel header. Picks up the M16 QFrame#panelHeader
    // QSS rule (tokens.qss line 56) automatically.
    impl_->header = new QFrame(this);
    impl_->header->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(impl_->header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    impl_->headerLabel = new QLabel(tr("Signals"), impl_->header);
    impl_->headerLabel->setProperty("class", QLatin1String("heading"));
    headerLayout->addWidget(impl_->headerLabel);
    headerLayout->addStretch();
    layout->addWidget(impl_->header);

    auto* body = new QVBoxLayout();
    body->setContentsMargins(4, 4, 4, 4);
    body->setSpacing(4);

    impl_->filterEdit = new QLineEdit(this);
    impl_->filterEdit->setPlaceholderText(tr("Filter signals…"));
    body->addWidget(impl_->filterEdit);

    impl_->countLabel = new QLabel(this);
    impl_->countLabel->setObjectName(QStringLiteral("signalSelectorCount"));
    impl_->countLabel->setProperty("class", QLatin1String("caption"));
    body->addWidget(impl_->countLabel);

    impl_->tree = new QTreeWidget(this);
    impl_->tree->setHeaderHidden(true);
    impl_->tree->header()->setStretchLastSection(true);
    body->addWidget(impl_->tree);

    layout->addLayout(body);

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
    int visible = 0;
    int total = static_cast<int>(impl_->leaves.size());
    for (auto& [signalId, leaf] : impl_->leaves) {
        const bool match = substring.isEmpty() || signalId.contains(substring, Qt::CaseInsensitive);
        leaf->setHidden(!match);
        if (match) {
            ++visible;
        }
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

    // Count label: "N / M signals" when filtered, "M signals" when
    // unfiltered, "No signals available" when empty.
    if (total == 0) {
        impl_->countLabel->setText(tr("No signals available"));
    } else if (substring.isEmpty()) {
        impl_->countLabel->setText(tr("%1 signals").arg(total));
    } else {
        impl_->countLabel->setText(tr("%1 / %2 signals").arg(visible).arg(total));
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

    // M17 S3 — sort signal ids before insertion so tree order is
    // deterministic regardless of registry insertion sequence.
    // (ADR-014 already guarantees signalIds() returns a stable
    // QStringList, but explicit sort here removes the cross-module
    // coupling and makes the SignalSelector's own contract clear.)
    QStringList signalIds = registry_->signalIds();
    signalIds.sort();

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

    // M17 S3 — top-level item order may not match `groups` map
    // order because QTreeWidget appends items in the order
    // `new QTreeWidgetItem(tree)` is called. Re-sort by re-taking
    // and re-inserting in alphabetical group-label order.
    if (impl_->tree->topLevelItemCount() > 1) {
        QList<QTreeWidgetItem*> items;
        items.reserve(impl_->tree->topLevelItemCount());
        while (impl_->tree->topLevelItemCount() > 0) {
            items.push_back(impl_->tree->takeTopLevelItem(0));
        }
        std::sort(items.begin(), items.end(),
                  [](QTreeWidgetItem* a, QTreeWidgetItem* b) { return a->text(0) < b->text(0); });
        for (auto* item : items) {
            impl_->tree->addTopLevelItem(item);
            item->setExpanded(true);
        }
    }

    // Re-apply the existing filter so newly-added items respect it
    // (this also refreshes the count label).
    setFilter(impl_->filter);
}

}  // namespace signalforge::chart
