// src/video/video_view.hpp
#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPainter;
class QWheelEvent;

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

    /// Replace the displayed frame and schedule a repaint. Ignored while frozen.
    void setFrame(const QImage& frame);

    /// Drop the current frame; the placeholder is shown until the next `setFrame`.
    /// Clears regardless of the frozen state.
    void clearFrame();

    /// Freeze the display: while frozen, `setFrame` is ignored so the current
    /// image stays put for inspection.
    void setFrozen(bool frozen);
    [[nodiscard]] bool isFrozen() const noexcept;

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

    /// Reset zoom to fit and clear any pan.
    void resetView();

    /// Current zoom factor relative to fit (1.0 = fit-to-window).
    [[nodiscard]] double zoom() const noexcept;

    /// Map a widget coordinate to the source image pixel under it. Returns
    /// (-1,-1) when there is no frame or the point is outside the displayed image.
    [[nodiscard]] QPoint mapToImage(const QPoint& widgetPos) const;

    [[nodiscard]] QSize sizeHint() const override;

signals:
    /// Emitted on hover: the source pixel under the cursor and its colour.
    void pixelProbed(const QPoint& imagePos, const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void paintOverlay(QPainter& p);
    [[nodiscard]] double fitScale() const;
    [[nodiscard]] double currentScale() const;
    [[nodiscard]] QRectF targetRect() const;

    QImage frame_;
    QString placeholder_ = QStringLiteral("Waiting for video stream…");
    QString overlay_;
    bool overlayVisible_ = true;
    bool frozen_ = false;
    double zoom_ = 1.0;
    QPointF pan_{0.0, 0.0};
    bool dragging_ = false;
    QPointF lastDragPos_{0.0, 0.0};
};

}  // namespace signalforge::video
