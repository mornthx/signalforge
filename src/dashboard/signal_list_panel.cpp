// src/dashboard/signal_list_panel.cpp

#include "dashboard/signal_list_panel.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/dashboard.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace signalforge::dashboard {

namespace {

constexpr auto kDerivedDriverId = "expression-engine";  ///< M7 virtual driver.

/// Group label for a signal id ("Derived" for derived signals,
/// "Driver: <driverId>" otherwise). Mirrors chart::SignalSelector's
/// grouping so the two read identically.
QString groupLabelFor(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QStringLiteral("Derived");
    }
    const QString driver = signalId.left(slash);
    if (driver == QLatin1String(kDerivedDriverId)) {
        return QStringLiteral("Derived");
    }
    return QStringLiteral("Driver: %1").arg(driver);
}

QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

}  // namespace

SignalListPanel::SignalListPanel(signalforge::buffer::SignalBufferRegistry& registry, Dashboard& dashboard,
                                 QWidget* parent)
    : QWidget(parent), registry_(&registry), dashboard_(&dashboard) {
    setObjectName(QStringLiteral("signalListPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    auto* headerLabel = new QLabel(tr("Signals"), header);
    headerLabel->setProperty("class", QLatin1String("heading"));
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch(1);
    layout->addWidget(header);

    auto* body = new QVBoxLayout();
    body->setContentsMargins(4, 4, 4, 4);
    body->setSpacing(4);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText(tr("Filter signals…"));
    body->addWidget(filterEdit_);

    countLabel_ = new QLabel(this);
    countLabel_->setObjectName(QStringLiteral("signalListCount"));
    countLabel_->setProperty("class", QLatin1String("caption"));
    body->addWidget(countLabel_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    body->addWidget(tree_);

    layout->addLayout(body);

    connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString& text) { setFilter(text); });

    connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column != 0 || item == nullptr) {
            return;
        }
        const QString signalId = item->data(0, Qt::UserRole).toString();
        if (signalId.isEmpty()) {
            return;  // a driver group row, not a leaf.
        }
        const bool checked = item->checkState(0) == Qt::Checked;
        if (checked) {
            dashboard_->addSignal(signalId);
        } else {
            dashboard_->removeSignalEverywhere(signalId);
        }
        Q_EMIT signalToggled(signalId, checked);
    });

    refresh();
}

SignalListPanel::~SignalListPanel() = default;

void SignalListPanel::setFilter(const QString& substring) {
    filter_ = substring;
    int visible = 0;
    int total = 0;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* group = tree_->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem* leaf = group->child(j);
            ++total;
            const bool match =
                substring.isEmpty() || leaf->data(0, Qt::UserRole).toString().contains(substring, Qt::CaseInsensitive);
            leaf->setHidden(!match);
            if (match) {
                ++visible;
                anyVisible = true;
            }
        }
        group->setHidden(!anyVisible);
    }
    if (total == 0) {
        countLabel_->setText(tr("No signals available"));
    } else if (substring.isEmpty()) {
        countLabel_->setText(tr("%1 signals").arg(total));
    } else {
        countLabel_->setText(tr("%1 / %2 signals").arg(visible).arg(total));
    }
}

void SignalListPanel::refresh() {
    // Block itemChanged during programmatic rebuild so we don't route
    // synthetic toggles into the dashboard.
    const QSignalBlocker blocker(tree_);
    tree_->clear();

    QStringList ids = registry_->signalIds();
    ids.sort();

    std::map<QString, QTreeWidgetItem*> groups;
    for (const QString& signalId : ids) {
        auto* buf = registry_->bufferFor(signalId);
        if (buf == nullptr) {
            continue;
        }
        const QString groupLabel = groupLabelFor(signalId);
        QTreeWidgetItem* group = nullptr;
        if (auto it = groups.find(groupLabel); it != groups.end()) {
            group = it->second;
        } else {
            group = new QTreeWidgetItem(tree_);
            group->setText(0, groupLabel);
            group->setExpanded(true);
            groups[groupLabel] = group;
        }

        const auto& meta = buf->metadata();
        QString leafText = meta.name.isEmpty() ? fieldName(signalId) : meta.name;
        if (!meta.unit.isEmpty()) {
            leafText += QStringLiteral("  (") + meta.unit + QStringLiteral(")");
        }
        auto* leaf = new QTreeWidgetItem(group);
        leaf->setText(0, leafText);
        leaf->setData(0, Qt::UserRole, signalId);
        leaf->setFlags(leaf->flags() | Qt::ItemIsUserCheckable);
        // Source of truth: the dashboard, not a chart.
        leaf->setCheckState(0, dashboard_->showsSignal(signalId) ? Qt::Checked : Qt::Unchecked);
    }

    setFilter(filter_);
}

}  // namespace signalforge::dashboard
