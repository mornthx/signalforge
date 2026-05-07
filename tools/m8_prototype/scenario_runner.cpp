// tools/m8_prototype/scenario_runner.cpp
#include "scenario_runner.hpp"

#include <algorithm>
#include <cstdio>

namespace {

double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double pos = p * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(pos);
    const std::size_t hi = std::min(lo + 1, sorted.size() - 1);
    const double frac = pos - static_cast<double>(lo);
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

}  // namespace

FrameTimingRecorder::FrameTimingRecorder(QQuickWindow* window, int targetFrames, QObject* parent)
    : QObject(parent), window_(window), targetFrames_(targetFrames) {
    intervalMs_.reserve(static_cast<std::size_t>(targetFrames));
    renderMs_.reserve(static_cast<std::size_t>(targetFrames));
    // frameSwapped: the swapchain has actually swapped. Use this to
    // measure end-to-end frame interval (vsync-bound).
    QObject::connect(window_, &QQuickWindow::frameSwapped, this, [this] { onAfterRendering(); },
                     Qt::DirectConnection);
    // beforeRendering / afterRendering: bracket the SG render-loop work
    // (no vsync wait). Use this to measure pure SG/GPU cost.
    QObject::connect(window_, &QQuickWindow::beforeRendering, this, [this] { recordRenderStart(); },
                     Qt::DirectConnection);
    QObject::connect(window_, &QQuickWindow::afterRendering, this, [this] { recordRenderEnd(); },
                     Qt::DirectConnection);
}

void FrameTimingRecorder::recordRenderStart() {
    renderStart_ = std::chrono::steady_clock::now();
}

void FrameTimingRecorder::recordRenderEnd() {
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double, std::milli>(now - renderStart_).count();
    renderMs_.push_back(dt);
}

void FrameTimingRecorder::onAfterRendering() {
    const auto now = std::chrono::steady_clock::now();
    if (!primed_) {
        lastFrame_ = now;
        primed_ = true;
        return;
    }
    const double dtMs = std::chrono::duration<double, std::milli>(now - lastFrame_).count();
    lastFrame_ = now;
    intervalMs_.push_back(dtMs);
    if (static_cast<int>(intervalMs_.size()) >= targetFrames_) {
        complete_ = true;
        emit completed();
    }
}

ScenarioResult FrameTimingRecorder::finalize() const {
    ScenarioResult r;
    r.framesRecorded = static_cast<int>(intervalMs_.size());
    if (!intervalMs_.empty()) {
        std::vector<double> sorted = intervalMs_;
        std::sort(sorted.begin(), sorted.end());
        r.p50Ms = percentile(sorted, 0.50);
        r.p95Ms = percentile(sorted, 0.95);
        r.p99Ms = percentile(sorted, 0.99);
        r.maxMs = sorted.back();
        double sum = 0.0;
        for (double v : sorted) {
            sum += v;
        }
        r.meanMs = sum / static_cast<double>(sorted.size());
        constexpr double kFrameBudgetMs = 33.333;
        constexpr double kDropThresholdMs = kFrameBudgetMs * 1.5;
        for (double v : intervalMs_) {
            if (v > kDropThresholdMs) {
                ++r.droppedFrames;
            }
        }
    }
    if (!renderMs_.empty()) {
        std::vector<double> rsort = renderMs_;
        std::sort(rsort.begin(), rsort.end());
        r.renderSamples = static_cast<int>(rsort.size());
        r.renderP50Ms = percentile(rsort, 0.50);
        r.renderP95Ms = percentile(rsort, 0.95);
        r.renderP99Ms = percentile(rsort, 0.99);
        r.renderMaxMs = rsort.back();
    }
    return r;
}
