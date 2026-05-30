// src/inspect/raw_packet_view.cpp

#include "inspect/raw_packet_view.hpp"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <optional>

namespace signalforge::inspect {

namespace {

constexpr int kRefreshIntervalMs = 100;  ///< 10 Hz poll.

enum Column { kNo = 0, kTime, kSource, kProto, kLen, kInfo, kColumnCount };
enum TreeColumn { kField = 0, kValue, kType, kBytes, kTreeColumnCount };

// Roles carrying a tree node's byte range, read back to highlight the hex pane.
constexpr int kByteOffsetRole = Qt::UserRole + 1;
constexpr int kByteLengthRole = Qt::UserRole + 2;

/// Compact hex of the first `maxBytes` bytes for the Info column.
QString hexPreview(const QByteArray& payload, int maxBytes = 16) {
    const QByteArray head = payload.left(maxBytes);
    QString out = QString::fromLatin1(head.toHex(' '));
    if (payload.size() > maxBytes) {
        out += QStringLiteral(" …");
    }
    return out;
}

/// Display string for a node's byte range: "5–6", "9", or "9 [bit 0]".
QString bytesLabel(const signalforge::decoder::DissectedField& f) {
    QString span;
    if (f.byteLength <= 1) {
        span = QString::number(f.byteOffset);
    } else {
        span = QStringLiteral("%1–%2").arg(f.byteOffset).arg(f.byteOffset + f.byteLength - 1);
    }
    if (f.bitCount > 0) {
        span += (f.bitCount == 1) ? QStringLiteral(" [bit %1]").arg(f.bitStart)
                                  : QStringLiteral(" [bits %1..%2]").arg(f.bitStart).arg(f.bitStart + f.bitCount - 1);
    }
    return span;
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
    connect(table_, &QTableWidget::itemSelectionChanged, this, &RawPacketView::onCurrentRowChanged);
    split->addWidget(table_);

    // Dissection tree (Wireshark's middle "packet details" pane).
    tree_ = new QTreeWidget(split);
    tree_->setObjectName(QStringLiteral("rawDissectionTree"));
    tree_->setColumnCount(kTreeColumnCount);
    tree_->setHeaderLabels({tr("Field"), tr("Value"), tr("Type"), tr("Bytes")});
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setRootIsDecorated(true);
    tree_->header()->setStretchLastSection(true);
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
        if (item == nullptr) {
            hexView_->setExtraSelections({});
            return;
        }
        const QVariant off = item->data(kField, kByteOffsetRole);
        const QVariant len = item->data(kField, kByteLengthRole);
        if (off.isValid() && len.isValid()) {
            highlightBytes(off.toInt(), len.toInt());
        } else {
            hexView_->setExtraSelections({});
        }
    });
    split->addWidget(tree_);

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
    split->setStretchFactor(0, 4);  // packet list
    split->setStretchFactor(1, 4);  // dissection tree
    split->setStretchFactor(2, 3);  // hex pane

    body->addWidget(split, 1);
    layout->addLayout(body, 1);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(kRefreshIntervalMs);
    connect(refreshTimer_, &QTimer::timeout, this, &RawPacketView::refresh);
    refreshTimer_->start();

    refresh();
}

RawPacketView::~RawPacketView() = default;

void RawPacketView::setDissectorProvider(DissectorProvider provider) {
    dissectorProvider_ = std::move(provider);
    updateDissectionForCurrentRow();  // re-dissect the current selection, if any
}

void RawPacketView::redissectCurrent() {
    updateDissectionForCurrentRow();
}

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

void RawPacketView::onCurrentRowChanged() {
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        hexView_->clear();
        byteHexPos_.clear();
        byteAsciiPos_.clear();
        tree_->clear();
        return;
    }
    rebuildHex(rows_[static_cast<std::size_t>(row)].payload);
    updateDissectionForCurrentRow();
}

void RawPacketView::rebuildHex(const QByteArray& payload) {
    const int n = payload.size();
    byteHexPos_.assign(n, -1);
    byteAsciiPos_.assign(n, -1);

    QString out;
    for (int off = 0; off < n; off += 16) {
        out += QStringLiteral("%1  ").arg(off, 4, 16, QLatin1Char('0'));  // offset label + 2 spaces
        for (int i = 0; i < 16; ++i) {
            const int idx = off + i;
            if (idx < n) {
                byteHexPos_[idx] = static_cast<int>(out.size());
                out += QStringLiteral("%1 ").arg(static_cast<unsigned char>(payload[idx]), 2, 16, QLatin1Char('0'));
            } else {
                out += QStringLiteral("   ");
            }
            if (i == 7) {
                out += QLatin1Char(' ');
            }
        }
        out += QStringLiteral(" |");
        for (int i = 0; i < 16; ++i) {
            const int idx = off + i;
            if (idx < n) {
                const auto byte = static_cast<unsigned char>(payload[idx]);
                byteAsciiPos_[idx] = static_cast<int>(out.size());
                out += (byte >= 0x20 && byte < 0x7F) ? QChar(byte) : QChar(QLatin1Char('.'));
            }
        }
        out += QStringLiteral("|\n");
    }
    hexView_->setPlainText(out);
    hexView_->setExtraSelections({});
}

void RawPacketView::highlightBytes(int offset, int length) {
    QList<QTextEdit::ExtraSelection> sels;
    QColor bg = palette().highlight().color();
    bg.setAlpha(110);
    QTextCharFormat fmt;
    fmt.setBackground(bg);

    const int total = static_cast<int>(byteHexPos_.size());
    for (int i = offset; i < offset + length && i < total; ++i) {
        if (i < 0 || byteHexPos_[i] < 0) {
            continue;
        }
        QTextEdit::ExtraSelection hexSel;
        hexSel.format = fmt;
        QTextCursor hc(hexView_->document());
        hc.setPosition(byteHexPos_[i]);
        hc.setPosition(byteHexPos_[i] + 2, QTextCursor::KeepAnchor);
        hexSel.cursor = hc;
        sels.append(hexSel);

        if (byteAsciiPos_[i] >= 0) {
            QTextEdit::ExtraSelection asciiSel;
            asciiSel.format = fmt;
            QTextCursor ac(hexView_->document());
            ac.setPosition(byteAsciiPos_[i]);
            ac.setPosition(byteAsciiPos_[i] + 1, QTextCursor::KeepAnchor);
            asciiSel.cursor = ac;
            sels.append(asciiSel);
        }
    }
    hexView_->setExtraSelections(sels);

    // Scroll the first highlighted byte into view.
    if (offset >= 0 && offset < total && byteHexPos_[offset] >= 0) {
        QTextCursor c(hexView_->document());
        c.setPosition(byteHexPos_[offset]);
        hexView_->setTextCursor(c);
        hexView_->ensureCursorVisible();
    }
}

void RawPacketView::showTreePlaceholder(const QString& message) {
    tree_->clear();
    auto* item = new QTreeWidgetItem(tree_);
    item->setFirstColumnSpanned(true);
    item->setText(kField, message);
    item->setFlags(Qt::ItemIsEnabled);  // visible but not selectable
}

void RawPacketView::addFieldItem(QTreeWidgetItem* parent, const signalforge::decoder::DissectedField& field) {
    auto* item = (parent == nullptr) ? new QTreeWidgetItem(tree_) : new QTreeWidgetItem(parent);
    item->setText(kField, field.name);
    QString value = field.value;
    if (!field.unit.isEmpty() && !field.truncated) {
        value += QLatin1Char(' ') + field.unit;
    }
    item->setText(kValue, value);
    item->setText(kType, field.typeLabel);
    item->setText(kBytes, bytesLabel(field));
    item->setData(kField, kByteOffsetRole, field.byteOffset);
    item->setData(kField, kByteLengthRole, field.byteLength);
    if (field.truncated) {
        item->setForeground(kValue, palette().color(QPalette::Disabled, QPalette::Text));
    }
    for (const auto& child : field.children) {
        addFieldItem(item, child);
    }
}

void RawPacketView::updateDissectionForCurrentRow() {
    tree_->clear();
    const int row = table_->currentRow();
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return;
    }
    const CapturedFrame& frame = rows_[static_cast<std::size_t>(row)];

    if (!dissectorProvider_) {
        showTreePlaceholder(tr("No decoder schema — raw bytes only."));
        return;
    }
    const signalforge::decoder::FrameDissector* dissector = dissectorProvider_(frame.source);
    if (dissector == nullptr) {
        showTreePlaceholder(tr("No decoder schema for “%1” — raw bytes only.").arg(frame.source));
        return;
    }

    const signalforge::decoder::Dissection dis = dissector->dissect(frame.payload);
    if (!dis.matched) {
        showTreePlaceholder(dis.diagnostic.isEmpty() ? tr("No layout matched this frame.") : dis.diagnostic);
        return;
    }
    if (!dis.diagnostic.isEmpty()) {
        auto* note = new QTreeWidgetItem(tree_);
        note->setFirstColumnSpanned(true);
        note->setText(kField, dis.diagnostic);
        note->setFlags(Qt::ItemIsEnabled);
    }
    for (const auto& field : dis.fields) {
        addFieldItem(nullptr, field);
    }
    tree_->expandAll();
}

void RawPacketView::selectRow(int row) {
    if (row >= 0 && row < table_->rowCount()) {
        table_->selectRow(row);
        onCurrentRowChanged();
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
