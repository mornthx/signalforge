// src/dashboard/plot_view.hpp
#pragma once

#include "buffer/signal_buffer.hpp"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <optional>
#include <utility>
#include <vector>

namespace signalforge::buffer {
class SignalBufferRegistry;
}
namespace signalforge::chart {
class TimeAxisManager;
}

namespace signalforge::dashboard {

/// A readable time-series plot painted with `QPainter` (no QML/RHI), so
/// it is theme-aware and headless-capturable. Renders N signals over a
/// shared `TimeAxisManager` window with Y-axis tick labels + unit,
/// X-axis time labels, an in-canvas colored legend, ≥2 px lines, a live
/// "now" cursor, and optional per-signal normalization.
///
/// Parallel to the frozen legacy `chart::Chart` (which is untouched).
class PlotView : public QWidget {
    Q_OBJECT

public:
    PlotView(signalforge::buffer::SignalBufferRegistry& registry, signalforge::chart::TimeAxisManager& timeAxis,
             QWidget* parent = nullptr);
    ~PlotView() override;

    PlotView(const PlotView&) = delete;
    PlotView& operator=(const PlotView&) = delete;

    void addSignal(const QString& signalId);
    void removeSignal(const QString& signalId);
    [[nodiscard]] QStringList signalIds() const;
    [[nodiscard]] bool hasSignal(const QString& signalId) const;

    /// Normalize each signal to its own observed range (compare shapes of
    /// different-scale signals). Off by default (shared real-value Y axis).
    void setPerSignalNormalize(bool on);
    [[nodiscard]] bool perSignalNormalize() const noexcept {
        return perSignalNormalize_;
    }

    /// Pin the shared Y axis to [min, max]; pass nullopt for either to
    /// auto-range that bound from the data.
    void setYRange(std::optional<double> min, std::optional<double> max);

    /// Re-query the visible window and repaint. Driven by the dashboard
    /// refresh tick and by `TimeAxisManager::rangeChanged`.
    void refresh();

    // --- test / diagnostic accessors ---
    [[nodiscard]] QColor colorFor(const QString& signalId) const;
    [[nodiscard]] std::pair<double, double> computedYRange() const noexcept {
        return {yMin_, yMax_};
    }
    [[nodiscard]] bool hasData() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Series {
        QString id;
        QColor color;
        std::vector<signalforge::buffer::SignalSample> samples;  ///< cached by refresh()
        double seriesMin = 0.0;                                  ///< per-signal observed (for normalize)
        double seriesMax = 0.0;
    };

    [[nodiscard]] int indexOf(const QString& signalId) const;
    void recomputeRanges();

    signalforge::buffer::SignalBufferRegistry* registry_;
    signalforge::chart::TimeAxisManager* timeAxis_;
    std::vector<Series> series_;
    bool perSignalNormalize_ = false;
    std::optional<double> explicitMin_;
    std::optional<double> explicitMax_;
    double yMin_ = -1.0;  ///< shared range used by the last refresh
    double yMax_ = 1.0;
    int nextColorIndex_ = 0;
};

}  // namespace signalforge::dashboard
