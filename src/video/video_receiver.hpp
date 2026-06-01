// src/video/video_receiver.hpp
#pragma once

#include "video/video_protocol.hpp"
#include "video/video_types.hpp"

#include <QImage>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <memory>

class QThread;

namespace signalforge::video {

/// Register `QImage` and `signalforge::video::VideoStats` as Qt metatypes so
/// they can cross threads via `Qt::QueuedConnection`. Idempotent; called from
/// the `VideoUdpReceiver` constructor.
void registerMetatypes();

class VideoReassemblyWorker;  // defined in the .cpp (lives on the IO thread)

/// Receives the board's raw-RGB24 video over UDP and reassembles it.
///
/// A dedicated IO thread owns a `QUdpSocket` bound to the video port with a
/// large `SO_RCVBUF`. Incoming ZVID datagrams are reassembled inline; each
/// complete frame is emitted as a detached `QImage` (`Format_RGB888`), and
/// throughput/health is reported periodically via `statsUpdated`. Incomplete
/// frames are dropped (counted), so packet loss lowers fps without tearing.
///
/// This path is deliberately separate from the scalar `UdpDriver`/`FramePipeline`
/// (which wraps one `RawFrame` per datagram): at ~570 Mbps / ~48k datagrams/s
/// the video stream gets its own cadence and buffer (see
/// `memory/heterogeneous_frame_rates`).
///
/// Thread safety: all four signals are emitted on the thread that constructed
/// the receiver (queued from the IO thread); connect them to UI objects freely.
class VideoUdpReceiver : public QObject {
    Q_OBJECT

public:
    explicit VideoUdpReceiver(quint16 port = kDefaultVideoPort, QObject* parent = nullptr);
    ~VideoUdpReceiver() override;

    /// The port most recently requested via the constructor or `rebind()`.
    [[nodiscard]] quint16 port() const noexcept;

public slots:
    /// Bind the socket and begin receiving. Idempotent. Emits `runningChanged(true)`
    /// on success or `errorOccurred` on bind failure.
    void start();

    /// Stop receiving and close the socket. Idempotent. Emits `runningChanged(false)`.
    void stop();

    /// Rebind to a new port (stop, then start on `port`).
    void rebind(quint16 port);

signals:
    /// A complete, detached RGB888 frame is ready for display.
    void frameReady(const QImage& frame);

    /// A periodic throughput/health snapshot.
    void statsUpdated(const signalforge::video::VideoStats& stats);

    /// A non-fatal receiver error (e.g. bind failure) with a human-readable message.
    void errorOccurred(const QString& message);

    /// The socket became bound (`true`) or was torn down (`false`).
    void runningChanged(bool running);

private:
    std::unique_ptr<QThread> thread_;
    std::unique_ptr<VideoReassemblyWorker> worker_;
    quint16 port_;
};

}  // namespace signalforge::video
