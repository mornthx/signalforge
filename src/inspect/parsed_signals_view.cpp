// src/inspect/parsed_signals_view.cpp

#include "inspect/parsed_signals_view.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <optional>
#include <utility>

namespace signalforge::inspect {

namespace {

constexpr int kRefreshIntervalMs = 100;  ///< 10 Hz value/age refresh.

enum Column { kName = 0, kSource, kValue, kUnit, kType, kAge, kColumnCount };

QString sourceOf(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash > 0 ? signalId.left(slash) : signalId;
}

QString fieldOf(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

QString typeName(signalforge::decoder::SignalType type) {
    switch (type) {
    case signalforge::decoder::SignalType::Bool:
        return QStringLiteral("bool");
    case signalforge::decoder::SignalType::Int64:
        return QStringLiteral("int");
    case signalforge::decoder::SignalType::Double:
        return QStringLiteral("double");
    case signalforge::decoder::SignalType::String:
        return QStringLiteral("string");
    }
    return QStringLiteral("?");
}

/// Numeric where possible, else the formatted string — for the filter engine.
signalforge::query::FieldValue toFieldValue(const signalforge::decoder::SignalValue& v) {
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b;
    }
    const double d = signalforge::dashboard::valueToDouble(v);
    if (!std::isnan(d)) {
        return d;
    }
    return signalforge::dashboard::formatValue(v, 3);
}

QString formatAge(std::chrono::nanoseconds age) {
    const double seconds = std::chrono::duration<double>(age).count();
    if (seconds < 1.0) {
        return QStringLiteral("%1 ms").arg(static_cast<int>(seconds * 1000.0));
    }
    return QStringLiteral("%1 s").arg(seconds, 0, 'f', 1);
}

}  // namespace

ParsedSignalsView::ParsedSignalsView(signalforge::buffer::SignalBufferRegistry& registry, QWidget* parent)
    : QWidget(parent), registry_(&registry) {
    setObjectName(QStringLiteral("parsedSignalsView"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    auto* title = new QLabel(tr("Parsed signals"), header);
    title->setProperty("class", QLatin1String("heading"));
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    countLabel_ = new QLabel(header);
    countLabel_->setProperty("class", QLatin1String("caption"));
    headerLayout->addWidget(countLabel_);
    layout->addWidget(header);

    auto* body = new QVBoxLayout();
    body->setContentsMargins(6, 6, 6, 6);
    body->setSpacing(4);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setObjectName(QStringLiteral("parsedFilterEdit"));
    filterEdit_->setPlaceholderText(tr("Filter, e.g.  source == udp:rig && value > 20"));
    connect(filterEdit_, &QLineEdit::textChanged, this, &ParsedSignalsView::setFilter);
    body->addWidget(filterEdit_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(kColumnCount);
    table_->setHorizontalHeaderLabels({tr("Name"), tr("Source"), tr("Value"), tr("Unit"), tr("Type"), tr("Age")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(kName, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kSource, QHeaderView::ResizeToContents);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &ParsedSignalsView::showRowMenu);
    body->addWidget(table_, 1);

    layout->addLayout(body, 1);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &ParsedSignalsView::refresh);
    refreshTimer_->start();

    refresh();
}

ParsedSignalsView::~ParsedSignalsView() = default;

void ParsedSignalsView::setFilter(const QString& text) {
    auto result = signalforge::query::FilterExpr::parse(text);
    if (result.ok()) {
        filter_ = *result.expr;
        filterValid_ = true;
        filterEdit_->setProperty("invalid", false);
        filterEdit_->setToolTip(QString());
    } else {
        filter_ = signalforge::query::FilterExpr{};  // match-all while invalid
        filterValid_ = false;
        filterEdit_->setProperty("invalid", true);
        filterEdit_->setToolTip(tr("Filter error: %1").arg(result.error));
    }
    // Re-polish so a QSS [invalid="true"] rule can re-style the field.
    filterEdit_->style()->unpolish(filterEdit_);
    filterEdit_->style()->polish(filterEdit_);
    applyFilter();
}

void ParsedSignalsView::rebuild(const QStringList& ids) {
    cachedIds_ = ids;
    rows_.clear();
    rows_.reserve(static_cast<std::size_t>(ids.size()));
    table_->setRowCount(ids.size());

    int row = 0;
    for (const QString& id : ids) {
        auto* buf = registry_->bufferFor(id);
        RowData data;
        data.id = id;
        data.source = sourceOf(id);
        data.value = QStringLiteral("—");
        if (buf != nullptr) {
            const auto& meta = buf->metadata();
            data.name = meta.name.isEmpty() ? fieldOf(id) : meta.name;
            data.unit = meta.unit;
            data.type = typeName(meta.type);
        } else {
            data.name = fieldOf(id);
        }

        table_->setItem(row, kName, new QTableWidgetItem(data.name));
        table_->item(row, kName)->setData(Qt::UserRole, id);
        table_->setItem(row, kSource, new QTableWidgetItem(data.source));
        table_->setItem(row, kValue, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kUnit, new QTableWidgetItem(data.unit));
        table_->setItem(row, kType, new QTableWidgetItem(data.type));
        table_->setItem(row, kAge, new QTableWidgetItem(QStringLiteral("—")));

        rows_.push_back(std::move(data));
        ++row;
    }
}

void ParsedSignalsView::refresh() {
    QStringList ids = registry_->signalIds();
    ids.sort();
    if (ids != cachedIds_) {
        rebuild(ids);
    }

    for (std::size_t r = 0; r < rows_.size(); ++r) {
        auto* buf = registry_->bufferFor(rows_[r].id);
        const int row = static_cast<int>(r);
        if (buf == nullptr) {
            continue;
        }
        const auto latest = buf->queryLatestOne();
        if (latest.has_value()) {
            rows_[r].value = toFieldValue(latest->value);
            table_->item(row, kValue)->setText(signalforge::dashboard::formatValue(latest->value, 3));
            table_->item(row, kAge)->setText(formatAge(latest->age));
        } else {
            rows_[r].value = QStringLiteral("—");
            table_->item(row, kValue)->setText(QStringLiteral("—"));
            table_->item(row, kAge)->setText(QStringLiteral("—"));
        }
    }

    applyFilter();
}

void ParsedSignalsView::applyFilter() {
    int visible = 0;
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        const RowData& data = rows_[r];
        const auto lookup = [&data](const QString& f) -> std::optional<signalforge::query::FieldValue> {
            const QString lf = f.toLower();
            if (lf == QLatin1String("id")) {
                return signalforge::query::FieldValue(data.id);
            }
            if (lf == QLatin1String("name")) {
                return signalforge::query::FieldValue(data.name);
            }
            if (lf == QLatin1String("source")) {
                return signalforge::query::FieldValue(data.source);
            }
            if (lf == QLatin1String("unit")) {
                return signalforge::query::FieldValue(data.unit);
            }
            if (lf == QLatin1String("type")) {
                return signalforge::query::FieldValue(data.type);
            }
            if (lf == QLatin1String("value")) {
                return data.value;
            }
            return std::nullopt;
        };
        const bool show = filter_.matches(lookup);
        table_->setRowHidden(static_cast<int>(r), !show);
        if (show) {
            ++visible;
        }
    }
    const int total = static_cast<int>(rows_.size());
    if (total == 0) {
        countLabel_->setText(tr("no signals"));
    } else if (filter_.isEmpty()) {
        countLabel_->setText(tr("%1 signals").arg(total));
    } else {
        countLabel_->setText(tr("%1 / %2 signals").arg(visible).arg(total));
    }
}

QMenu* ParsedSignalsView::buildAddToDashboardMenu(const QString& signalId) {
    auto* menu = new QMenu(this);
    auto* addMenu = menu->addMenu(tr("Add to dashboard"));
    struct TypeEntry {
        QString label;
        QString token;
    };
    const TypeEntry entries[] = {{tr("Numeric"), QStringLiteral("numeric")},
                                 {tr("State"), QStringLiteral("state")},
                                 {tr("Plot"), QStringLiteral("plot")},
                                 {tr("Bar"), QStringLiteral("bar")},
                                 {tr("Gauge"), QStringLiteral("gauge")}};
    for (const auto& e : entries) {
        const QString token = e.token;
        connect(addMenu->addAction(e.label), &QAction::triggered, this,
                [this, signalId, token]() { Q_EMIT addToDashboardRequested(signalId, token); });
    }
    return menu;
}

void ParsedSignalsView::showRowMenu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return;
    }
    QMenu* menu = buildAddToDashboardMenu(rows_[static_cast<std::size_t>(row)].id);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(table_->viewport()->mapToGlobal(pos));
}

int ParsedSignalsView::totalRowCount() const {
    return static_cast<int>(rows_.size());
}

int ParsedSignalsView::visibleRowCount() const {
    int visible = 0;
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (!table_->isRowHidden(r)) {
            ++visible;
        }
    }
    return visible;
}

}  // namespace signalforge::inspect
