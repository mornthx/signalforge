// src/inspect/raw_packet_view.hpp
#pragma once

#include "decode/frame_dissector.hpp"
#include "inspect/raw_frame_tap.hpp"
#include "query/filter_expr.hpp"

#include <QString>
#include <QWidget>
#include <chrono>
#include <functional>
#include <optional>
#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace signalforge::inspect {

/// Tier 1 of the workbench (原报文): a Wireshark-style raw-packet view over a
/// `RawFrameTap`. Three stacked panes:
///   1. a packet list (No · Time · Source · Proto · Len · Info),
///   2. a schema-driven **dissection tree** for the selected frame (field →
///      decoded value → byte range, with bitfield children), and
///   3. a hex pane that highlights the byte range of the selected field.
/// A display-filter bar (the `signalforge_query` engine) narrows the list over
/// packet metadata (`no/source/proto/len/seq/hex/ascii`) **and** the selected
/// schema's dissected field names (e.g. `temperature > 20`, `status.alarm == 1`,
/// dotted `parent.child` for bit slices) — resolved per row via the injected
/// dissector, so field filters only cost dissection when actually used.
///
/// Dissection is decoupled: the app injects a `DissectorProvider` that maps a
/// frame's `source` to the `FrameDissector` for that driver's schema. With no
/// provider (or no schema for a source) the tree shows a placeholder and the
/// list + hex panes still work — the Raw view never depends on a decoder.
class RawPacketView : public QWidget {
    Q_OBJECT

public:
    /// Resolves a captured frame's `source` to the dissector for its schema,
    /// or nullptr when that source has no decoder schema. The returned pointer
    /// must outlive the call (the app owns the dissectors).
    using DissectorProvider = std::function<const signalforge::decoder::FrameDissector*(const QString& source)>;

    explicit RawPacketView(RawFrameTap& tap, QWidget* parent = nullptr);
    ~RawPacketView() override;

    RawPacketView(const RawPacketView&) = delete;
    RawPacketView& operator=(const RawPacketView&) = delete;

    /// Apply a display filter. Invalid expressions are flagged and leave all
    /// rows visible.
    void setFilter(const QString& text);

    /// Set the filter-bar text (and apply it). Unlike `setFilter`, this updates
    /// the visible bar so a programmatic filter (e.g. a cross-tier drill-through)
    /// is transparent and editable by the user.
    void setFilterText(const QString& text);

    /// Pull new frames from the tap, append rows, and re-apply the filter.
    void refresh();

    /// Inject the schema-dissection source (see `DissectorProvider`).
    void setDissectorProvider(DissectorProvider provider);

    /// Re-run dissection for the currently selected packet. Call after the set
    /// of available dissectors changes (e.g. a connection's schema loads).
    void redissectCurrent();

    // --- test accessors ---
    [[nodiscard]] int totalRowCount() const;
    [[nodiscard]] int visibleRowCount() const;
    [[nodiscard]] bool filterValid() const {
        return filterValid_;
    }
    [[nodiscard]] QLineEdit* filterEdit() const {
        return filterEdit_;
    }
    [[nodiscard]] QTableWidget* table() const {
        return table_;
    }
    [[nodiscard]] QPlainTextEdit* hexView() const {
        return hexView_;
    }
    [[nodiscard]] QTreeWidget* dissectionTree() const {
        return tree_;
    }
    /// Select a packet row (updates the dissection tree + hex pane), for tests.
    void selectRow(int row);

private:
    void appendRow(const CapturedFrame& frame);
    void applyFilter();
    void onCurrentRowChanged();
    void updateDissectionForCurrentRow();
    void rebuildHex(const QByteArray& payload);
    void highlightBytes(int offset, int length);
    void addFieldItem(QTreeWidgetItem* parent, const signalforge::decoder::DissectedField& field);
    void showTreePlaceholder(const QString& message);
    [[nodiscard]] bool rowMatches(const CapturedFrame& frame) const;

    RawFrameTap* tap_;
    QLineEdit* filterEdit_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QPlainTextEdit* hexView_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    DissectorProvider dissectorProvider_;
    std::vector<int> byteHexPos_;    ///< absolute char pos of each byte's hex pair in the dump (-1 if absent)
    std::vector<int> byteAsciiPos_;  ///< absolute char pos of each byte's ASCII glyph in the dump

    std::vector<CapturedFrame> rows_;  ///< parallel to table rows
    std::uint64_t lastIndex_ = 0;      ///< highest tap index appended
    std::size_t maxRows_ = 5000;
    std::optional<std::chrono::steady_clock::time_point> firstRecvAt_;
    signalforge::query::FilterExpr filter_;
    bool filterValid_ = true;
};

}  // namespace signalforge::inspect
