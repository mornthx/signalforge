// tests/unit/video/zvid_test_util.hpp
#pragma once

#include "video/video_protocol.hpp"

#include <QByteArray>
#include <cstdint>

/// Test-only helpers for synthesizing ZVID datagrams and frames.
namespace sfvideotest {

inline void appendLe16(QByteArray& b, std::uint16_t v) {
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

inline void appendLe32(QByteArray& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.append(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

/// Build a 24-byte little-endian ZVID header.
inline QByteArray makeHeader(std::uint32_t magic, std::uint32_t frame, std::uint32_t offset, std::uint32_t frameBytes,
                             std::uint16_t w, std::uint16_t h, std::uint16_t bpp, std::uint16_t chunkLen) {
    QByteArray b;
    appendLe32(b, magic);
    appendLe32(b, frame);
    appendLe32(b, offset);
    appendLe32(b, frameBytes);
    appendLe16(b, w);
    appendLe16(b, h);
    appendLe16(b, bpp);
    appendLe16(b, chunkLen);
    return b;  // exactly 24 bytes
}

/// Build a full datagram (header + pixel chunk).
inline QByteArray makeDatagram(std::uint32_t frame, std::uint32_t offset, std::uint32_t frameBytes, std::uint16_t w,
                               std::uint16_t h, const QByteArray& chunk,
                               std::uint32_t magic = signalforge::video::kZvidMagic, std::uint16_t bpp = 3) {
    QByteArray d = makeHeader(magic, frame, offset, frameBytes, w, h, bpp, static_cast<std::uint16_t>(chunk.size()));
    d.append(chunk);
    return d;
}

/// Deterministic RGB24 pixel buffer for a w*h frame (varies by seed).
inline QByteArray makeFramePixels(int w, int h, std::uint8_t seed) {
    QByteArray px;
    px.resize(w * h * 3);
    for (int i = 0; i < px.size(); ++i) {
        px[i] = static_cast<char>(static_cast<std::uint8_t>((i * 131 + seed * 17) & 0xFF));
    }
    return px;
}

}  // namespace sfvideotest
