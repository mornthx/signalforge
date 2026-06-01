// src/video/video_view.hpp
#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QWidget>

class QPainter;

namespace signalforge::video {

/// Lightweight live-video display widget.
///
/// Renders the most recent frame aspect-preserving and centered on a dark
/// backdrop; when no frame is set it draws a muted placeholder message. The
/// widget is repaint-on-set (its own cadence — driven by frame arrival, not a
/// periodic timer), matching the receiver's push model.
class VideoView : public QWidget {
    Q_OBJECT

public:
    explicit VideoView(QWidget* parent = nullptr);

    /// Replace the displayed frame and schedule a repaint.
    void setFrame(const QImage& frame);

    /// Drop the current frame; the placeholder is shown until the next `setFrame`.
    void clearFrame();

    /// The most recently set frame (null `QImage` if none / cleared).
    [[nodiscard]] QImage currentFrame() const;

    /// Whether a (non-null) frame is currently held.
    [[nodiscard]] bool hasFrame() const noexcept;

    /// Text drawn when no frame is held.
    void setPlaceholderText(const QString& text);
    [[nodiscard]] QString placeholderText() const;

    /// Stats overlay drawn (when visible) in the top-left over the frame.
    /// Multi-line text is supported (split on '\n').
    void setOverlayText(const QString& text);
    [[nodiscard]] QString overlayText() const;
    void setOverlayVisible(bool visible);
    [[nodiscard]] bool isOverlayVisible() const noexcept;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintOverlay(QPainter& p);

    QImage frame_;
    QString placeholder_ = QStringLiteral("Waiting for video stream…");
    QString overlay_;
    bool overlayVisible_ = true;
};

}  // namespace signalforge::video
