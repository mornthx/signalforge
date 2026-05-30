// src/inspect/raw_packet_view.cpp

#include "inspect/raw_packet_view.hpp"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <optional>

namespace signalforge::inspect {

namespace {

constexpr int kRefreshIntervalMs = 100;  ///< 10 Hz poll.

enum Column { kNo = 0, kTime, kSource, kProto, kLen, kInfo, kColumnCount };

/// Compact hex of the first `maxBytes` bytes for the Info column.
QString hexPreview(const QByteArray& payload, int maxBytes = 16) {
    const QByteArray head = payload.left(maxBytes);
    QString out = QString::fromLatin1(head.toHex(' '));
    if (payload.size() > maxBytes) {
        out += QStringLiteral(" …");
    }
    return out;
}

/// Wireshark-style hex dump: `0000  xx xx ..  |ascii|` rows of 16 bytes.
QString hexDump(const QByteArray& payload) {
    QString out;
    const int n = payload.size();
    for (int off = 0; off < n; off += 16) {
        QString hex;
        QString ascii;
        for (int i = 0; i < 16; ++i) {
            if (off + i < n) {
                const auto byte = static_cast<unsigned char>(payload[off + i]);
                hex += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0'));
                ascii += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar(QLatin1Char('.'));
            } else {
                hex += QStringLiteral("   ");
            }
            if (i == 7) {
                hex += QLatin1Char(' ');
            }
        }
        out += QStringLiteral("%1  %2 |%3|\n").arg(off, 4, 16, QLatin1Char('0')).arg(hex, ascii);
    }
    return out;
}

}  // namespace

RawPacketView::RawPacketView(RawFrameTap& tap, QWidget* parent) : QWidget(parent), tap_(&tap) {
    setObjectName(QStringLiteral("rawPacketView"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("panelHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    auto* title = new QLabel(tr("Raw packets"), header);
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
    filterEdit_->setObjectName(QStringLiteral("rawFilterEdit"));
    filterEdit_->setPlaceholderText(tr("Filter, e.g.  source == udp:rig && len > 8 && hex contains ff"));
    connect(filterEdit_, &QLineEdit::textChanged, this, &RawPacketView::setFilter);
    body->addWidget(filterEdit_);

    auto* split = new QSplitter(Qt::Vertical, this);

    table_ = new QTableWidget(split);
    table_->setColumnCount(kColumnCount);
    table_->setHorizontalHeaderLabels({tr("No."), tr("Time"), tr("Source"), tr("Proto"), tr("Len"), tr("Info")});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &RawPacketView::updateHexForCurrentRow);
    split->addWidget(table_);

    hexView_ = new QPlainTextEdit(split);
    hexView_->setObjectName(QStringLiteral("rawHexView"));
    hexView_->setReadOnly(true);
    {
        QFont mono(QStringLiteral("monospace"));
        mono.setStyleHint(QFont::Monospace);
        hexView_->setFont(mono);
    }
    hexView_->setPlaceholderText(tr("Select a packet to inspect its bytes"));
    split->addWidget(hexView_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);

    body->addWidget(split, 1);
    layout->addLayout(body, 1);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &RawPacketView::refresh);
    refreshTimer_->start();

    refresh();
}

RawPacketView::~RawPacketView() = default;

void RawPacketView::setFilter(const QString& text) {
    auto result = signalforge::query::FilterExpr::parse(text);
    if (result.ok()) {
        filter_ = *result.expr;
        filterValid_ = true;
        filterEdit_->setProperty("invalid", false);
        filterEdit_->setToolTip(QString());
    } else {
        filter_ = signalforge::query::FilterExpr{};
        filterValid_ = false;
        filterEdit_->setProperty("invalid", true);
        filterEdit_->setToolTip(tr("Filter error: %1").arg(result.error));
    }
    filterEdit_->style()->unpolish(filterEdit_);
    filterEdit_->style()->polish(filterEdit_);
    applyFilter();
}

bool RawPacketView::rowMatches(const CapturedFrame& frame) const {
    const auto lookup = [&frame](const QString& f) -> std::optional<signalforge::query::FieldValue> {
        const QString lf = f.toLower();
        if (lf == QLatin1String("no")) {
            return signalforge::query::FieldValue(static_cast<double>(frame.index));
        }
        if (lf == QLatin1String("source")) {
            return signalforge::query::FieldValue(frame.source);
        }
        if (lf == QLatin1String("proto") || lf == QLatin1String("protocol")) {
            return signalforge::query::FieldValue(frame.protocol);
        }
        if (lf == QLatin1String("len")) {
            return signalforge::query::FieldValue(static_cast<double>(frame.payload.size()));
        }
        if (lf == QLatin1String("seq")) {
            return signalforge::query::FieldValue(static_cast<double>(frame.sequence));
        }
        if (lf == QLatin1String("hex")) {
            return signalforge::query::FieldValue(QString::fromLatin1(frame.payload.toHex()));
        }
        if (lf == QLatin1String("ascii")) {
            return signalforge::query::FieldValue(QString::fromLatin1(frame.payload));
        }
        return std::nullopt;
    };
    return filter_.matches(lookup);
}

void RawPacketView::appendRow(const CapturedFrame& frame) {
    if (!firstRecvAt_.has_value()) {
        firstRecvAt_ = frame.recvAt;
    }
    const double seconds = std::chrono::duration<double>(frame.recvAt - *firstRecvAt_).count();

    const int row = table_->rowCount();
    table_->insertRow(row);
    table_->setItem(row, kNo, new QTableWidgetItem(QString::number(frame.index)));
    table_->setItem(row, kTime, new QTableWidgetItem(QString::number(seconds, 'f', 3)));
    table_->setItem(row, kSource, new QTableWidgetItem(frame.source));
    table_->setItem(row, kProto, new QTableWidgetItem(frame.protocol));
    table_->setItem(row, kLen, new QTableWidgetItem(QString::number(frame.payload.size())));
    table_->setItem(row, kInfo, new QTableWidgetItem(hexPreview(frame.payload)));
    rows_.push_back(frame);

    // Cap displayed history (oldest first out).
    while (rows_.size() > maxRows_) {
        rows_.erase(rows_.begin());
        table_->removeRow(0);
    }
}

void RawPacketView::refresh() {
    const auto fresh = tap_->since(lastIndex_);
    for (const CapturedFrame& f : fresh) {
        appendRow(f);
        lastIndex_ = std::max(lastIndex_, f.index);
    }
    applyFilter();
}

void RawPacketView::applyFilter() {
    int visible = 0;
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        const bool show = rowMatches(rows_[r]);
        table_->setRowHidden(static_cast<int>(r), !show);
        if (show) {
            ++visible;
        }
    }
    const int total = static_cast<int>(rows_.size());
    if (total == 0) {
        countLabel_->setText(tr("no packets"));
    } else if (filter_.isEmpty()) {
        countLabel_->setText(tr("%1 packets").arg(total));
    } else {
        countLabel_->setText(tr("%1 / %2 packets").arg(visible).arg(total));
    }
}

void RawPacketView::updateHexForCurrentRow() {
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        hexView_->clear();
        return;
    }
    hexView_->setPlainText(hexDump(rows_[static_cast<std::size_t>(row)].payload));
}

void RawPacketView::selectRow(int row) {
    if (row >= 0 && row < table_->rowCount()) {
        table_->selectRow(row);
        updateHexForCurrentRow();
    }
}

int RawPacketView::totalRowCount() const {
    return static_cast<int>(rows_.size());
}

int RawPacketView::visibleRowCount() const {
    int visible = 0;
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (!table_->isRowHidden(r)) {
            ++visible;
        }
    }
    return visible;
}

}  // namespace signalforge::inspect
