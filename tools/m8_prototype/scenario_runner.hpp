// tools/m8_prototype/scenario_runner.hpp
//
// Scenario harness shared across the 4 benchmark cases. Sets up a
// QQuickWindow, parents N LineChartItems, drives a 30 Hz redraw via
// QTimer, and records per-frame wall time on QQuickWindow's
// afterRendering signal. After `frameCount_` frames it computes
// p50 / p95 / p99 and quits the application.

#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QTimer>
#include <chrono>
#include <vector>

class LineChartItem;

struct ScenarioResult {
    // Frame interval (vsync-bound at 30 Hz target = ~33.3 ms).
    int framesRecorded = 0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double p99Ms = 0.0;
    double maxMs = 0.0;
    double meanMs = 0.0;
    int droppedFrames = 0;  ///< frames where interval was > 50 ms (1.5x budget)
    double stdDevMs = 0.0;  ///< sample std dev of frame interval

    // Pure render-loop cost (beforeRendering→afterRendering on render thread).
    // This is the SG/GPU work cost without vsync wait — the actual headroom.
    int renderSamples = 0;
    double renderP50Ms = 0.0;
    double renderP95Ms = 0.0;
    double renderP99Ms = 0.0;
    double renderMaxMs = 0.0;
};

class FrameTimingRecorder : public QObject {
    Q_OBJECT
public:
    FrameTimingRecorder(QQuickWindow* window, int targetFrames, QObject* parent = nullptr);

    [[nodiscard]] ScenarioResult finalize() const;
    [[nodiscard]] bool isComplete() const { return complete_; }

signals:
    void completed();

private slots:
    void onAfterRendering();

    void recordRenderStart();
    void recordRenderEnd();

private:
    QQuickWindow* window_ = nullptr;
    int targetFrames_ = 0;
    std::vector<double> intervalMs_;
    std::vector<double> renderMs_;
    std::chrono::steady_clock::time_point lastFrame_{};
    std::chrono::steady_clock::time_point renderStart_{};
    bool primed_ = false;
    bool complete_ = false;
};
