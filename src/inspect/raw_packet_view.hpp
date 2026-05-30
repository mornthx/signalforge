// src/inspect/raw_packet_view.hpp
#pragma once

#include "inspect/raw_frame_tap.hpp"
#include "query/filter_expr.hpp"

#include <QString>
#include <QWidget>
#include <chrono>
#include <optional>
#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTimer;

namespace signalforge::inspect {

/// Tier 1 of the workbench (原报文): a Wireshark-style raw-packet view over a
/// `RawFrameTap`. A packet list (No · Time · Source · Proto · Len · Info) above
/// a hex pane for the selected frame, with a display-filter bar (the
/// `signalforge_query` engine over `no/source/proto/len/seq/hex/ascii`).
///
/// Polls the tap on a timer; the schema-driven dissection tree is a documented
/// follow-up (M32).
class RawPacketView : public QWidget {
    Q_OBJECT

public:
    explicit RawPacketView(RawFrameTap& tap, QWidget* parent = nullptr);
    ~RawPacketView() override;

    RawPacketView(const RawPacketView&) = delete;
    RawPacketView& operator=(const RawPacketView&) = delete;

    /// Apply a display filter. Invalid expressions are flagged and leave all
    /// rows visible.
    void setFilter(const QString& text);

    /// Pull new frames from the tap, append rows, and re-apply the filter.
    void refresh();

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
    /// Select a packet row (updates the hex pane), for interaction tests.
    void selectRow(int row);

private:
    void appendRow(const CapturedFrame& frame);
    void applyFilter();
    void updateHexForCurrentRow();
    [[nodiscard]] bool rowMatches(const CapturedFrame& frame) const;

    RawFrameTap* tap_;
    QLineEdit* filterEdit_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPlainTextEdit* hexView_ = nullptr;
    QTimer* refreshTimer_ = nullptr;

    std::vector<CapturedFrame> rows_;  ///< parallel to table rows
    std::uint64_t lastIndex_ = 0;      ///< highest tap index appended
    std::size_t maxRows_ = 5000;
    std::optional<std::chrono::steady_clock::time_point> firstRecvAt_;
    signalforge::query::FilterExpr filter_;
    bool filterValid_ = true;
};

}  // namespace signalforge::inspect
