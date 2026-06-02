// src/video/video_protocol.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

/// @file
/// Wire format of the board's raw-RGB24 video-over-UDP stream ("ZVID").
///
/// The XCZU15EG board broadcasts each 1280x720 RGB24 frame as a sequence of
/// UDP datagrams. Every datagram begins with a fixed 24-byte little-endian
/// header followed by a chunk of pixel bytes. Reassembly copies each chunk to
/// `frame[offset : offset+chunkLen]`; a frame is complete once the received
/// byte count reaches `frameBytes`. See
/// `/home/shuai/Videos/zu15eg/zu15eg_comm/docs/视频UDP流-应用接入.md`.
///
/// This header is intentionally Qt-free so the parser can be unit-tested in
/// isolation.
namespace signalforge::video {

/// Datagram magic, ASCII 'ZVID' read little-endian (`0x5A564944`). Filters
/// foreign UDP traffic sharing the bind port.
inline constexpr std::uint32_t kZvidMagic = 0x5A564944U;

/// Size of the fixed ZVID datagram header, in bytes.
inline constexpr std::size_t kZvidHeaderSize = 24;

/// Default UDP port the board broadcasts video on (`169.254.255.255:5004`).
inline constexpr std::uint16_t kDefaultVideoPort = 5004;

/// Bytes per pixel for the only pixel format M35 decodes (RGB24).
inline constexpr std::uint16_t kRgb24Bpp = 3;

/// Decoded ZVID datagram header (host representation, all fields native).
struct ZvidHeader {
    std::uint32_t magic = 0;       ///< Must equal `kZvidMagic`.
    std::uint32_t frame = 0;       ///< Frame number; identical across one frame's datagrams.
    std::uint32_t offset = 0;      ///< Byte offset of this chunk within the full RGB24 frame.
    std::uint32_t frameBytes = 0;  ///< Full frame size = width*height*bpp.
    std::uint16_t width = 0;       ///< Frame width in pixels.
    std::uint16_t height = 0;      ///< Frame height in pixels.
    std::uint16_t bpp = 0;         ///< Bytes per pixel (3 for RGB24).
    std::uint16_t chunkLen = 0;    ///< Pixel bytes following the header in this datagram.
};

namespace detail {

[[nodiscard]] inline std::uint16_t readLe16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8));
}

[[nodiscard]] inline std::uint32_t readLe32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace detail

/// Parse a ZVID header from the front of a datagram.
///
/// @param data Pointer to the datagram bytes (may be null).
/// @param len  Datagram length in bytes.
/// @return The decoded header, or `std::nullopt` if the datagram is shorter
///         than `kZvidHeaderSize` or its magic does not match `kZvidMagic`.
///         Structural sanity (e.g. `frameBytes == width*height*bpp`) is the
///         caller's responsibility.
[[nodiscard]] inline std::optional<ZvidHeader> parseZvidHeader(const std::uint8_t* data, std::size_t len) noexcept {
    if (data == nullptr || len < kZvidHeaderSize) {
        return std::nullopt;
    }
    ZvidHeader h;
    h.magic = detail::readLe32(data + 0);
    if (h.magic != kZvidMagic) {
        return std::nullopt;
    }
    h.frame = detail::readLe32(data + 4);
    h.offset = detail::readLe32(data + 8);
    h.frameBytes = detail::readLe32(data + 12);
    h.width = detail::readLe16(data + 16);
    h.height = detail::readLe16(data + 18);
    h.bpp = detail::readLe16(data + 20);
    h.chunkLen = detail::readLe16(data + 22);
    return h;
}

}  // namespace signalforge::video
