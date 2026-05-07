// src/drivers/driver_configs.hpp
#pragma once

#include <QString>
#include <QtGlobal>
#include <chrono>
#include <cstdint>

namespace signalforge::drivers {

/// Serial port configuration.
///
/// Value type. Passed by value to `SerialDriver`'s constructor. `open()`
/// validates fields and returns `DriverErrorCode::ConfigInvalid` if any
/// rejected value is encountered (negative bauds, out-of-range dataBits,
/// unknown parity/flowControl strings).
struct SerialConfig {
    QString device;                                ///< e.g. "/dev/ttyUSB0" or "/tmp/ttyV0"
    qint32 baudRate = 115200;                      ///< Standard rates: 9600, 38400, 115200, 921600
    int dataBits = 8;                              ///< 5, 6, 7, or 8
    QString parity = QStringLiteral("none");       ///< "none", "even", "odd", "mark", "space"
    int stopBits = 1;                              ///< 1 or 2
    QString flowControl = QStringLiteral("none");  ///< "none", "hardware", "software"
};

/// TCP connection configuration.
///
/// Value type. `host` is resolved at `open()` time (DNS lookup on the IO
/// thread). `port == 0` is rejected as `ConfigInvalid`.
struct TcpConfig {
    QString host;                                    ///< Hostname or IPv4/IPv6 literal
    quint16 port = 0;                                ///< 1..65535; 0 rejected in open()
    std::chrono::milliseconds connectTimeout{5000};  ///< open() timeout
};

/// UDP endpoint configuration.
///
/// Value type. At least one of `{localBindPort != 0 || localBindAddress
/// != "0.0.0.0"}` (bind intent) or `{!remoteHost.isEmpty()}` (send intent)
/// must be set; otherwise `open()` returns `ConfigInvalid`. Multicast group
/// is optional; `multicastTtl` only applies when `multicastGroup` is
/// non-empty.
struct UdpConfig {
    QString localBindAddress = QStringLiteral("0.0.0.0");  ///< "0.0.0.0", "127.0.0.1", or specific
    quint16 localBindPort = 0;                             ///< 0 = OS-assigned
    QString remoteHost;                                    ///< Target for write()
    quint16 remotePort = 0;
    QString multicastGroup;    ///< Optional; e.g. "224.0.0.1"
    quint32 multicastTtl = 1;  ///< Valid only when multicastGroup is non-empty
};

/// Session replay configuration.
///
/// Value type. Full parsing is M9's responsibility; M3's ReplayDriver only
/// verifies file existence + 16-byte header non-zero check.
///
/// M9 additions (per M3-done.md "M9 adds: playback rate, loop mode" — M3
/// did not freeze this struct; the additions are purely additive):
///   - `playbackSpeed`: real-time multiplier (1.0 = real-time, 2.0 = 2x
///     fast, 0.5 = half-speed). Validated > 0 in M9 ReplayDriver.
///   - `loop`: when true, M9 ReplayDriver restarts at EOF.
struct ReplayConfig {
    QString sessionFilePath;     ///< Path to .sfreplay session file
    double playbackSpeed = 1.0;  ///< M9: real-time multiplier; rejected if <= 0
    bool loop = false;           ///< M9: restart at EOF when true
};

}  // namespace signalforge::drivers
