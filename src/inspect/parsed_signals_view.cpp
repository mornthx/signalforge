// src/inspect/parsed_signals_view.cpp

#include "inspect/parsed_signals_view.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QBrush>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QShowEvent>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace signalforge::inspect {

namespace {

constexpr int kRefreshIntervalMs = 100;  ///< 10 Hz value/age refresh.

// Quality freshness thresholds (M34 P2). Fixed for now; a later phase can
// derive them per-signal from the decoded period.
constexpr std::chrono::milliseconds kStaleAfter{1500};
constexpr std::chrono::milliseconds kBadAfter{5000};

constexpr int kSparkSamples = 48;                  ///< recent samples drawn in the mini-sparkline.
constexpr int kSparkPolyRole = Qt::UserRole + 1;   ///< QPolygonF (normalized 0..1) on the trend cell.
constexpr int kSparkColorRole = Qt::UserRole + 2;  ///< QColor for the trend line.

enum Column { kName = 0, kTrend, kQuality, kSource, kValue, kUnit, kRate, kType, kAge, kDash, kColumnCount };

/// Paints a normalized polyline (stored on the item via `kSparkPolyRole`) into
/// the Trend cell — a mini-sparkline of the signal's recent values.
class SparklineDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);  // background + selection
        const auto norm = index.data(kSparkPolyRole).value<QPolygonF>();
        if (norm.size() < 2) {
            return;
        }
        const QRectF r = QRectF(option.rect).adjusted(3, 3, -3, -3);
        QColor color = index.data(kSparkColorRole).value<QColor>();
        if (!color.isValid()) {
            color = option.palette.color(QPalette::Text);
        }
        QPolygonF poly;
        poly.reserve(norm.size());
        for (const QPointF& p : norm) {
            poly.append(QPointF(r.left() + p.x() * r.width(), r.bottom() - p.y() * r.height()));
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setClipRect(option.rect);
        painter->setPen(QPen(color, 1.2));
        painter->drawPolyline(poly);
        painter->restore();
    }
};

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
    table_->setHorizontalHeaderLabels({tr("Name"), tr("Trend"), tr("Quality"), tr("Source"), tr("Value"), tr("Unit"),
                                       tr("Rate"), tr("Type"), tr("Age"), tr("Dashboard")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(kName, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kQuality, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kSource, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kRate, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kDash, QHeaderView::ResizeToContents);
    table_->setColumnWidth(kTrend, 64);  // fixed mini-sparkline column
    table_->setItemDelegateForColumn(kTrend, new SparklineDelegate(table_));
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &ParsedSignalsView::showRowMenu);
    body->addWidget(table_, 1);

    layout->addLayout(body, 1);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    // Only do the periodic per-row work when the view is actually on screen.
    // When another segment (Raw / Dashboard) is showing, this view is hidden by
    // the QStackedWidget and there's nothing to repaint — skipping the tick
    // avoids needless 10 Hz table churn behind the dashboard.
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        if (isVisible()) {
            refresh();
        }
    });
    refreshTimer_->start();

    refresh();
}

ParsedSignalsView::~ParsedSignalsView() = default;

void ParsedSignalsView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
}

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

void ParsedSignalsView::setSignalColorProvider(std::function<QColor(const QString&)> provider) {
    signalColorProvider_ = std::move(provider);
}

void ParsedSignalsView::setQualityColorProvider(std::function<QColor(signalforge::workbench::Quality)> provider) {
    qualityColorProvider_ = std::move(provider);
}

void ParsedSignalsView::setDashboardMembershipProvider(std::function<bool(const QString&)> provider) {
    dashboardMembershipProvider_ = std::move(provider);
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
        if (signalColorProvider_) {
            // A QColor in DecorationRole renders as the signal-identity swatch.
            table_->item(row, kName)->setData(Qt::DecorationRole, signalColorProvider_(id));
        }
        table_->setItem(row, kTrend, new QTableWidgetItem(QString()));  // painted by SparklineDelegate
        table_->setItem(row, kQuality, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kSource, new QTableWidgetItem(data.source));
        table_->setItem(row, kValue, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kUnit, new QTableWidgetItem(data.unit));
        table_->setItem(row, kRate, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kType, new QTableWidgetItem(data.type));
        table_->setItem(row, kAge, new QTableWidgetItem(QStringLiteral("—")));
        table_->setItem(row, kDash, new QTableWidgetItem(QString()));

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
        // Refresh the identity swatch each tick so a runtime theme switch is
        // reflected (theme changes don't trigger a rebuild).
        if (signalColorProvider_) {
            table_->item(row, kName)->setData(Qt::DecorationRole, signalColorProvider_(rows_[r].id));
        }

        const auto latest = buf->queryLatestOne();
        auto quality = signalforge::workbench::Quality::Bad;  // no data → degraded
        if (latest.has_value()) {
            rows_[r].value = toFieldValue(latest->value);
            table_->item(row, kValue)->setText(signalforge::dashboard::formatValue(latest->value, 3));
            table_->item(row, kAge)->setText(formatAge(latest->age));
            quality = signalforge::workbench::qualityFromAge(latest->age, kStaleAfter, kBadAfter);
        } else {
            rows_[r].value = QStringLiteral("—");
            table_->item(row, kValue)->setText(QStringLiteral("—"));
            table_->item(row, kAge)->setText(QStringLiteral("—"));
        }

        const QString qLabel = signalforge::workbench::qualityName(quality);
        rows_[r].quality = qLabel;
        auto* qItem = table_->item(row, kQuality);
        qItem->setText(qLabel);
        if (qualityColorProvider_) {
            qItem->setForeground(QBrush(qualityColorProvider_(quality)));
        }

        const bool onDash = dashboardMembershipProvider_ && dashboardMembershipProvider_(rows_[r].id);
        rows_[r].onDashboard = onDash;
        table_->item(row, kDash)->setText(onDash ? tr("● on") : QString());

        // Rate (Hz) + mini-sparkline from the recent samples.
        const auto recent = buf->queryLatest(kSparkSamples);
        double rateHz = 0.0;
        double mn = std::numeric_limits<double>::infinity();
        double mx = -std::numeric_limits<double>::infinity();
        for (const auto& s : recent) {
            const double v = signalforge::dashboard::valueToDouble(s.value);
            if (std::isfinite(v)) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
        if (recent.size() >= 2) {
            const double spanS =
                std::chrono::duration<double>(recent.back().timestamp - recent.front().timestamp).count();
            if (spanS > 0.0) {
                rateHz = static_cast<double>(recent.size() - 1) / spanS;
            }
        }
        rows_[r].rateHz = rateHz;
        table_->item(row, kRate)->setText(rateHz > 0.0 ? tr("%1 Hz").arg(rateHz, 0, 'f', 0) : QStringLiteral("—"));

        QPolygonF spark;
        if (std::isfinite(mn) && std::isfinite(mx) && recent.size() >= 2) {
            const double span = (mx > mn) ? (mx - mn) : 1.0;
            const int n = static_cast<int>(recent.size());
            spark.reserve(n);
            for (int i = 0; i < n; ++i) {
                const double v = signalforge::dashboard::valueToDouble(recent[static_cast<std::size_t>(i)].value);
                const double y = std::isfinite(v) ? (v - mn) / span : 0.0;
                spark.append(QPointF(static_cast<double>(i) / (n - 1), y));
            }
        }
        auto* trendItem = table_->item(row, kTrend);
        trendItem->setData(kSparkPolyRole, QVariant::fromValue(spark));
        if (signalColorProvider_) {
            trendItem->setData(kSparkColorRole, signalColorProvider_(rows_[r].id));
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
            if (lf == QLatin1String("quality")) {
                return signalforge::query::FieldValue(data.quality);
            }
            if (lf == QLatin1String("rate")) {
                return signalforge::query::FieldValue(data.rateHz);
            }
            if (lf == QLatin1String("dashboard") || lf == QLatin1String("on_dashboard")) {
                return signalforge::query::FieldValue(data.onDashboard);
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

QMenu* ParsedSignalsView::buildRowMenu(const QString& signalId, bool onDashboard) {
    // M34 P2: the row action flips — Remove if the signal is already on the
    // dashboard, otherwise the "Add to dashboard ▸ <type>" submenu.
    if (!onDashboard) {
        return buildAddToDashboardMenu(signalId);
    }
    auto* menu = new QMenu(this);
    connect(menu->addAction(tr("Remove from dashboard")), &QAction::triggered, this,
            [this, signalId]() { Q_EMIT removeFromDashboardRequested(signalId); });
    return menu;
}

void ParsedSignalsView::showRowMenu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return;
    }
    const RowData& rd = rows_[static_cast<std::size_t>(row)];
    QMenu* menu = buildRowMenu(rd.id, rd.onDashboard);
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
