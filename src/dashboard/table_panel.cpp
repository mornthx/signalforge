// src/dashboard/table_panel.cpp

#include "dashboard/table_panel.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <chrono>

namespace signalforge::dashboard {

namespace {

enum Column { kSignal = 0, kValue = 1, kUnit = 2, kUpdated = 3, kColumnCount = 4 };

QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

QString formatAge(std::chrono::nanoseconds age) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
    if (ms < 1000) {
        return TablePanel::tr("%1ms ago").arg(ms);
    }
    return TablePanel::tr("%1s ago").arg(QString::number(static_cast<double>(ms) / 1000.0, 'f', 1));
}

}  // namespace

TablePanel::TablePanel(PanelConfig config, signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : Panel(std::move(config), parent), registry_(&registry) {
    if (config_.title.isEmpty()) {
        setHeaderTitle(tr("Live values"));
    }

    auto* body = new QWidget(this);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget(body);
    table_->setColumnCount(kColumnCount);
    table_->setHorizontalHeaderLabels({tr("Signal"), tr("Value"), tr("Unit"), tr("Updated")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_);

    setBody(body);
    rebuildRows();
}

TablePanel::~TablePanel() = default;

int TablePanel::rowForSignal(const QString& signalId) const {
    for (int row = 0; row < table_->rowCount(); ++row) {
        const auto* item = table_->item(row, kSignal);
        if (item != nullptr && item->data(Qt::UserRole).toString() == signalId) {
            return row;
        }
    }
    return -1;
}

void TablePanel::rebuildRows() {
    table_->setRowCount(config_.signalIds.size());
    int row = 0;
    for (const QString& signalId : config_.signalIds) {
        auto* buf = registry_->bufferFor(signalId);
        const QString name =
            (buf != nullptr && !buf->metadata().name.isEmpty()) ? buf->metadata().name : fieldName(signalId);
        const QString unit = buf != nullptr ? buf->metadata().unit : QString();

        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(Qt::UserRole, signalId);
        table_->setItem(row, kSignal, nameItem);
        table_->setItem(row, kValue, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kUnit, new QTableWidgetItem(unit));
        table_->setItem(row, kUpdated, new QTableWidgetItem(QStringLiteral("—")));
        ++row;
    }
    refresh();
}

void TablePanel::addSignal(const QString& signalId) {
    if (signalId.isEmpty() || config_.signalIds.contains(signalId)) {
        return;
    }
    config_.signalIds.append(signalId);
    rebuildRows();
}

void TablePanel::removeSignal(const QString& signalId) {
    if (config_.signalIds.removeAll(signalId) > 0) {
        rebuildRows();
    }
}

void TablePanel::refresh() {
    if (registry_ == nullptr) {
        return;
    }
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* nameItem = table_->item(row, kSignal);
        if (nameItem == nullptr) {
            continue;
        }
        const QString signalId = nameItem->data(Qt::UserRole).toString();
        auto* buf = registry_->bufferFor(signalId);
        auto* valueItem = table_->item(row, kValue);
        auto* updatedItem = table_->item(row, kUpdated);
        if (valueItem == nullptr || updatedItem == nullptr) {
            continue;
        }
        if (buf == nullptr) {
            valueItem->setText(QStringLiteral("—"));
            updatedItem->setText(QStringLiteral("—"));
            continue;
        }
        const auto latest = buf->queryLatestOne();
        if (!latest.has_value()) {
            valueItem->setText(QStringLiteral("—"));
            updatedItem->setText(QStringLiteral("—"));
            continue;
        }
        valueItem->setText(formatValue(latest->value, config_.decimals));
        updatedItem->setText(formatAge(latest->age));
    }
}

int TablePanel::rowCount() const {
    return table_ != nullptr ? table_->rowCount() : 0;
}

QString TablePanel::valueTextFor(const QString& signalId) const {
    const int row = rowForSignal(signalId);
    if (row < 0) {
        return QStringLiteral("—");
    }
    const auto* item = table_->item(row, kValue);
    return item != nullptr ? item->text() : QString();
}

}  // namespace signalforge::dashboard
