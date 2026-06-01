// src/video/video_page.hpp
#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QTimer;

namespace signalforge::video {

class VideoView;

/// Top-level "Video" page (a workbench mode, parallel to Connect / Inspect).
///
/// Hosts a control bar plus the live `VideoView`, and owns the display-side
/// state machine: disabled → waiting → streaming → stalled. It is fed by a
/// `VideoUdpReceiver`'s signals (wired by `MainWindow`); a watchdog flips the
/// view to a "stalled" placeholder when frames stop arriving while bound.
class VideoPage : public QWidget {
    Q_OBJECT

public:
    explicit VideoPage(QWidget* parent = nullptr);

    /// The embedded display widget (for wiring / tests).
    [[nodiscard]] VideoView* view() const noexcept;

    /// The control-bar container (later phases add screenshot / record buttons).
    [[nodiscard]] QHBoxLayout* controlBarLayout() const noexcept;

    /// Current one-line status text ("Disabled" / "Waiting…" / "Streaming" / "Stalled").
    [[nodiscard]] QString statusText() const;

    /// Override the stall watchdog timeout (default 3000 ms). Test seam.
    void setStallTimeoutMs(int ms);

public slots:
    /// A complete frame arrived — display it and re-arm the stall watchdog.
    void onFrameReady(const QImage& frame);

    /// The receiver bound (`true`) or stopped (`false`).
    void onRunningChanged(bool running);

    /// A receiver error — surfaced in the placeholder/status.
    void onError(const QString& message);

private slots:
    void onStallTimeout();

private:
    void setStatus(const QString& text);

    VideoView* view_ = nullptr;
    QHBoxLayout* controlBarLayout_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTimer* stallTimer_ = nullptr;
    bool running_ = false;
    int stallTimeoutMs_ = 3000;
};

}  // namespace signalforge::video
