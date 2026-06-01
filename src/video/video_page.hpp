// src/video/video_page.hpp
#pragma once

#include "video/color_correction.hpp"
#include "video/video_recorder.hpp"
#include "video/video_types.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <cstdint>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QTimer;
class QToolButton;

namespace signalforge::video {

class VideoView;
class ColorPanel;

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

    /// Current pixel-probe readout text ("(x,y) RGB(r,g,b)"), empty until hovered.
    [[nodiscard]] QString probeText() const;

    /// Override the stall watchdog timeout (default 3000 ms). Test seam.
    void setStallTimeoutMs(int ms);

    /// The "Stats" overlay toggle button (for wiring / tests).
    [[nodiscard]] QToolButton* statsToggleButton() const noexcept;

    /// The "Screenshot" button (for wiring / tests).
    [[nodiscard]] QToolButton* screenshotButton() const noexcept;

    /// Save the currently displayed frame to `path` as PNG. Returns false if
    /// there is no frame or the save fails. The non-dialog test seam.
    [[nodiscard]] bool saveScreenshot(const QString& path) const;

    /// The "Pause"/"Resume" button (for wiring / tests).
    [[nodiscard]] QToolButton* pauseButton() const noexcept;

    /// Whether the display is frozen (paused).
    [[nodiscard]] bool isPaused() const;

    /// The "Record"/"Stop" button (for wiring / tests).
    [[nodiscard]] QToolButton* recordButton() const noexcept;

    /// Whether a recording is in progress.
    [[nodiscard]] bool isRecording() const;

    /// Begin recording the live stream to `path` (uses the current frame size
    /// and the most recent fps estimate). Non-dialog test seam. Returns false
    /// if there is no frame or the recorder rejects the request.
    [[nodiscard]] bool startRecording(const QString& path, VideoRecorder::Format format);

    /// Stop the in-progress recording (no-op if not recording).
    void stopRecording();

    /// The "Color" panel toggle button and the panel itself (for wiring / tests).
    [[nodiscard]] QToolButton* colorButton() const noexcept;
    [[nodiscard]] ColorPanel* colorPanel() const noexcept;

    /// Current colour-correction parameters.
    [[nodiscard]] ColorParams colorParams() const;

    /// Apply colour-correction params (rebuilds the corrector + re-renders the
    /// current frame). Also the slot the slider panel drives.
    void setColorParams(const ColorParams& params);

    /// Whether the high-packet-loss rmem hint is currently shown.
    [[nodiscard]] bool isHintVisible() const;

public slots:
    /// A complete frame arrived — display it and re-arm the stall watchdog.
    void onFrameReady(const QImage& frame);

    /// The receiver bound (`true`) or stopped (`false`).
    void onRunningChanged(bool running);

    /// A periodic throughput/health snapshot — updates the overlay + rmem hint.
    void onStats(const signalforge::video::VideoStats& stats);

    /// A receiver error — surfaced in the placeholder/status.
    void onError(const QString& message);

private slots:
    void onStallTimeout();
    void onScreenshotClicked();
    void onPauseToggled(bool paused);
    void onPixelProbed(const QPoint& imagePos, const QColor& color);
    void onRecordClicked();
    void onRecordingStarted();
    void onRecordingStopped(bool ok);
    void updateElapsed();

private:
    void setStatus(const QString& text);
    void updateButtonStates();

    VideoView* view_ = nullptr;
    QHBoxLayout* controlBarLayout_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* probeLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QToolButton* pauseButton_ = nullptr;
    QToolButton* recordButton_ = nullptr;
    QToolButton* statsToggle_ = nullptr;
    QToolButton* screenshotButton_ = nullptr;
    QToolButton* colorButton_ = nullptr;
    QTimer* stallTimer_ = nullptr;
    QTimer* elapsedTimer_ = nullptr;
    QElapsedTimer recordClock_;
    VideoRecorder* recorder_ = nullptr;
    ColorPanel* colorPanel_ = nullptr;
    ColorCorrector corrector_;
    QImage lastRawFrame_;
    bool running_ = false;
    bool recordRaw_ = false;  ///< P4: record raw vs corrected (default corrected).
    int stallTimeoutMs_ = 3000;
    int recordFps_ = 25;
    std::uint64_t lastDelivered_ = 0;
    std::uint64_t lastDropped_ = 0;
};

}  // namespace signalforge::video
