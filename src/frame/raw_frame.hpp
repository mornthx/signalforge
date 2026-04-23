// src/frame/raw_frame.hpp
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <chrono>
#include <cstdint>
#include <optional>

namespace signalforge::frame {

/// Nanosecond-resolution monotonic timestamp. All internal ordering and
/// elapsed-time comparisons use this type. Producing drivers stamp frames as
/// close to the IO operation as possible.
using SteadyTimestamp = std::chrono::steady_clock::time_point;

/// Wall-clock timestamp for display only. Never use for comparisons or
/// ordering (subject to NTP corrections and manual edits).
using WallTimestamp = std::chrono::system_clock::time_point;

/// Stable identifier for the originating driver. Strings are cheap here
/// because QString is implicitly shared.
using SourceId = QString;

/// A raw byte-level frame from a driver.
///
/// Value type. Copyable: `payload` is a `QByteArray` whose implicit sharing
/// makes copies O(1) so long as no writer has triggered a deep copy.
/// `sourceId` and `protocolHint` are `QString`, also implicitly shared.
///
/// All timestamps are monotonic (`SteadyTimestamp`). The optional `deviceAt`
/// is translated to steady_clock by the concrete driver / M4 decoder if the
/// protocol delivers wall-clock device timestamps; M2 guarantees `deviceAt`
/// is never a wall-clock value at this boundary.
struct RawFrame {
    SourceId sourceId;  ///< Stable ID of the originating driver
    /// Raw bytes. Each byte is interpreted as unsigned char (0-255) at the
    /// M2 / frame-layer boundary. Higher-layer interpretation (signed ints,
    /// multi-byte values, endianness) is the decoder's responsibility in M4.
    /// Encoding is protocol-dependent.
    QByteArray payload;
    SteadyTimestamp recvAt{};  ///< When the bytes reached the IO thread
    /// If the frame carries a device-side timestamp, translated into
    /// steady_clock relative to the session's ClockOrigin. Absolute
    /// wall-clock device timestamps are translated by the decoder in M4, not
    /// at the driver level. M2 guarantee: this field is always steady-clock.
    std::optional<SteadyTimestamp> deviceAt;
    QString protocolHint;  ///< Optional; e.g. "serial", "tcp", "udp", "replay"

    // Reserved fields (populated by M3+; present now to avoid post-freeze
    // struct-layout changes).
    std::uint64_t sequenceNumber = 0;  ///< Driver-local monotonic counter
    std::uint32_t flags = 0;           ///< Bit-reserved; see FrameFlags in future milestones
};

/// Statistics atomicity model.
///
/// DriverStatistics (and the RxStats / TxStats components) is a point-in-time
/// snapshot returned by DriverInterface::statistics(). Individual counter
/// fields are updated on the IO thread as frames flow.
///
/// The driver implementation MUST use std::atomic<uint64_t> for each counter
/// field, and statistics() MUST read each field independently via atomic load.
/// Cross-field consistency is NOT guaranteed — a snapshot may show
/// framesTotal=101 while bytesTotal reflects only the first 100 frames.
/// This is acceptable because:
/// - The performance panel reads at 30Hz; cross-field skew is <33ms
/// - Monitoring consumers tolerate eventual consistency
///
/// Concrete drivers in M3+ MUST NOT use mutex-guarded stats blocks. Per-field
/// atomics avoid IO-thread contention with the UI-thread snapshot.

/// Receive-side statistics. All counters are monotonic increasing. Rates are
/// derived by the consumer via successive snapshots.
struct RxStats {
    std::uint64_t framesTotal = 0;
    std::uint64_t bytesTotal = 0;
    std::uint64_t framesDropped = 0;
    std::uint64_t errors = 0;
    SteadyTimestamp lastFrameAt{};
    std::uint32_t queueWatermarkPeak = 0;     ///< Peak since driver start (0-100)
    std::uint32_t queueWatermarkCurrent = 0;  ///< Current (0-100)
};

/// Transmit-side statistics. Same conventions as RxStats.
struct TxStats {
    std::uint64_t framesTotal = 0;
    std::uint64_t bytesTotal = 0;
    std::uint64_t failures = 0;
    SteadyTimestamp lastFrameAt{};
};

/// Combined snapshot for one driver. Returned by DriverInterface::statistics().
struct DriverStatistics {
    RxStats rx;
    TxStats tx;
    SteadyTimestamp snapshotAt{};
};

/// Register `RawFrame` as a Qt metatype so it can cross thread boundaries as
/// a signal argument. Must be called once per process before any
/// `DriverInterface` signal is connected. Idempotent: safe to call
/// repeatedly.
///
/// The M2 test harness calls this from its Catch2 listener; production
/// executables call it from `main()` before constructing any driver.
void registerMetatypes();

}  // namespace signalforge::frame

Q_DECLARE_METATYPE(signalforge::frame::RawFrame)
