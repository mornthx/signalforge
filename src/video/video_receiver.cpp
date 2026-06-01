// src/video/video_receiver.cpp
#include "video/video_receiver.hpp"

#include "observability/logging.hpp"
#include "platform/thread_utils.hpp"
#include "video/video_protocol.hpp"
#include "video/video_types.hpp"

#include <QAbstractSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QImage>
#include <QMetaObject>
#include <QNetworkDatagram>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QVariant>
#include <cstdint>
#include <cstring>

namespace signalforge::video {

namespace {
constexpr int kRcvBufBytes = 16 * 1024 * 1024;  ///< Requested SO_RCVBUF; kernel caps at net.core.rmem_max.
constexpr int kStatsIntervalMs = 500;           ///< Stats sampling cadence.
}  // namespace

void registerMetatypes() {
    qRegisterMetaType<QImage>("QImage");
    qRegisterMetaType<signalforge::video::VideoStats>("signalforge::video::VideoStats");
}

// =======================================================================
// VideoReassemblyWorker — lives on the IO thread
// =======================================================================
class VideoReassemblyWorker : public QObject {
    Q_OBJECT

public:
    explicit VideoReassemblyWorker(quint16 port) : port_(port) {}

public slots:
    void doStart() {
        if (socket_ != nullptr) {
            return;  // already bound
        }
        signalforge::platform::setCurrentThreadName(QStringLiteral("VideoRx-%1").arg(port_));

        socket_ = new QUdpSocket(this);
        connect(socket_, &QUdpSocket::readyRead, this, &VideoReassemblyWorker::onReadyRead);

        if (!socket_->bind(QHostAddress(QHostAddress::AnyIPv4), port_,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            const QString msg =
                QStringLiteral("video UDP bind failed on 0.0.0.0:%1: %2").arg(port_).arg(socket_->errorString());
            SF_LOG_ERROR("{}", msg.toStdString());
            socket_->deleteLater();
            socket_ = nullptr;
            emit errorOccurred(msg);
            return;
        }
        // Large receive buffer keeps 25 fps RGB24 bursts from overflowing the
        // socket queue. The kernel silently clamps this to net.core.rmem_max,
        // so a high host rmem_max is still required (surfaced as a UI hint).
        socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(kRcvBufBytes));

        resetReassembly();
        rxBytes_ = 0;
        framesDelivered_ = 0;
        framesDropped_ = 0;
        lastSampleFrames_ = 0;
        lastSampleBytes_ = 0;
        lastDeliveredW_ = 0;
        lastDeliveredH_ = 0;
        sampleClock_.start();

        if (statsTimer_ == nullptr) {
            statsTimer_ = new QTimer(this);
            statsTimer_->setInterval(kStatsIntervalMs);
            connect(statsTimer_, &QTimer::timeout, this, &VideoReassemblyWorker::emitStats);
        }
        statsTimer_->start();

        SF_LOG_INFO("video receiver bound on 0.0.0.0:{}", port_);
        emit runningChanged(true);
    }

    void doStop() {
        if (statsTimer_ != nullptr) {
            statsTimer_->stop();
        }
        if (socket_ != nullptr) {
            socket_->disconnect(this);
            socket_->close();
            socket_->deleteLater();
            socket_ = nullptr;
            emit runningChanged(false);
        }
    }

    void doRebind(quint16 port) {
        doStop();
        port_ = port;
        doStart();
    }

private slots:
    void onReadyRead() {
        if (socket_ == nullptr) {
            return;
        }
        while (socket_->hasPendingDatagrams()) {
            const QNetworkDatagram dg = socket_->receiveDatagram();
            if (!dg.isValid()) {
                continue;
            }
            const QByteArray data = dg.data();
            rxBytes_ += static_cast<std::uint64_t>(data.size());
            const auto hdr = parseZvidHeader(reinterpret_cast<const std::uint8_t*>(data.constData()),
                                             static_cast<std::size_t>(data.size()));
            if (!hdr) {
                continue;  // foreign packet or too short
            }
            handleChunk(*hdr, data);
        }
    }

    void emitStats() {
        const qint64 ms = sampleClock_.restart();
        if (ms <= 0) {
            return;
        }
        const double secs = static_cast<double>(ms) / 1000.0;
        VideoStats s;
        s.fps = static_cast<double>(framesDelivered_ - lastSampleFrames_) / secs;
        s.mbps = static_cast<double>(rxBytes_ - lastSampleBytes_) * 8.0 / 1.0e6 / secs;
        s.framesDelivered = framesDelivered_;
        s.framesDropped = framesDropped_;
        s.width = lastDeliveredW_;
        s.height = lastDeliveredH_;
        lastSampleFrames_ = framesDelivered_;
        lastSampleBytes_ = rxBytes_;
        emit statsUpdated(s);
    }

signals:
    void frameReady(const QImage& frame);
    void statsUpdated(const signalforge::video::VideoStats& stats);
    void errorOccurred(const QString& message);
    void runningChanged(bool running);

private:
    void resetReassembly() {
        curFrame_ = 0;
        frameBytes_ = 0;
        got_ = 0;
        width_ = 0;
        height_ = 0;
        haveFrame_ = false;
        delivered_ = false;
    }

    void handleChunk(const ZvidHeader& h, const QByteArray& data) {
        if (h.bpp != kRgb24Bpp) {
            if (!warnedBpp_) {
                SF_LOG_WARN("video: unsupported bpp={} (only RGB24 is decoded this milestone); dropping", h.bpp);
                warnedBpp_ = true;
            }
            return;
        }
        // Reject structurally inconsistent headers rather than trusting frameBytes blindly.
        const std::uint32_t expectBytes = static_cast<std::uint32_t>(h.width) * static_cast<std::uint32_t>(h.height) *
                                          static_cast<std::uint32_t>(h.bpp);
        if (h.frameBytes == 0 || h.frameBytes != expectBytes) {
            return;
        }

        if (!haveFrame_ || h.frame != curFrame_ || frameBytes_ != h.frameBytes) {
            // A new frame begins. If the previous one never completed, count a drop.
            if (haveFrame_ && !delivered_) {
                ++framesDropped_;
            }
            curFrame_ = h.frame;
            frameBytes_ = h.frameBytes;
            width_ = h.width;
            height_ = h.height;
            buf_.resize(static_cast<int>(h.frameBytes));
            got_ = 0;
            delivered_ = false;
            haveFrame_ = true;
        }

        if (delivered_) {
            return;  // late chunk for an already-delivered frame
        }

        const std::size_t off = h.offset;
        const std::size_t clen = h.chunkLen;
        if (clen == 0 || off + clen > static_cast<std::size_t>(frameBytes_)) {
            return;  // out-of-bounds chunk
        }
        if (static_cast<std::size_t>(data.size()) < kZvidHeaderSize + clen) {
            return;  // truncated datagram
        }
        std::memcpy(buf_.data() + off, data.constData() + kZvidHeaderSize, clen);
        got_ += clen;
        if (got_ >= static_cast<std::size_t>(frameBytes_)) {
            deliverFrame();
        }
    }

    void deliverFrame() {
        // Wrap the assembly buffer (explicit stride avoids QImage's 32-bit line
        // alignment requirement), then copy() to detach before crossing threads.
        const QImage view(reinterpret_cast<const uchar*>(buf_.constData()), width_, height_,
                          static_cast<qsizetype>(width_) * kRgb24Bpp, QImage::Format_RGB888);
        emit frameReady(view.copy());
        ++framesDelivered_;
        delivered_ = true;
        lastDeliveredW_ = width_;
        lastDeliveredH_ = height_;
    }

    quint16 port_;
    QUdpSocket* socket_ = nullptr;
    QTimer* statsTimer_ = nullptr;
    QElapsedTimer sampleClock_;

    // Reassembly state.
    QByteArray buf_;
    std::uint32_t curFrame_ = 0;
    std::uint32_t frameBytes_ = 0;
    std::size_t got_ = 0;
    int width_ = 0;
    int height_ = 0;
    bool haveFrame_ = false;
    bool delivered_ = false;
    bool warnedBpp_ = false;

    // Stats counters.
    std::uint64_t rxBytes_ = 0;
    std::uint64_t framesDelivered_ = 0;
    std::uint64_t framesDropped_ = 0;
    std::uint64_t lastSampleFrames_ = 0;
    std::uint64_t lastSampleBytes_ = 0;
    int lastDeliveredW_ = 0;
    int lastDeliveredH_ = 0;
};

// =======================================================================
// VideoUdpReceiver — facade on the constructing thread
// =======================================================================
VideoUdpReceiver::VideoUdpReceiver(quint16 port, QObject* parent) : QObject(parent), port_(port) {
    registerMetatypes();

    thread_ = std::make_unique<QThread>();
    thread_->setObjectName(QStringLiteral("VideoRx-%1").arg(port));

    worker_ = std::make_unique<VideoReassemblyWorker>(port);
    worker_->moveToThread(thread_.get());

    connect(worker_.get(), &VideoReassemblyWorker::frameReady, this, &VideoUdpReceiver::frameReady,
            Qt::QueuedConnection);
    connect(worker_.get(), &VideoReassemblyWorker::statsUpdated, this, &VideoUdpReceiver::statsUpdated,
            Qt::QueuedConnection);
    connect(worker_.get(), &VideoReassemblyWorker::errorOccurred, this, &VideoUdpReceiver::errorOccurred,
            Qt::QueuedConnection);
    connect(worker_.get(), &VideoReassemblyWorker::runningChanged, this, &VideoUdpReceiver::runningChanged,
            Qt::QueuedConnection);

    thread_->start();
}

VideoUdpReceiver::~VideoUdpReceiver() {
    if (thread_ && thread_->isRunning()) {
        QMetaObject::invokeMethod(worker_.get(), "doStop", Qt::QueuedConnection);
        thread_->quit();
        if (!thread_->wait(1000)) {
            SF_LOG_ERROR("video receiver IO thread did not exit within 1000ms; forcing terminate");
            thread_->terminate();
            thread_->wait();
        }
    }
    // Thread has stopped; the worker can be destroyed from this thread safely.
}

quint16 VideoUdpReceiver::port() const noexcept {
    return port_;
}

void VideoUdpReceiver::start() {
    QMetaObject::invokeMethod(worker_.get(), "doStart", Qt::QueuedConnection);
}

void VideoUdpReceiver::stop() {
    QMetaObject::invokeMethod(worker_.get(), "doStop", Qt::QueuedConnection);
}

void VideoUdpReceiver::rebind(quint16 port) {
    port_ = port;
    QMetaObject::invokeMethod(worker_.get(), "doRebind", Qt::QueuedConnection, Q_ARG(quint16, port));
}

}  // namespace signalforge::video

#include "video_receiver.moc"
