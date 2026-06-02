// tests/benchmark/bench_color_correction.cpp
//
// Measures ColorCorrector::apply throughput at 1280x720 RGB24 (M36 color
// correction on the GUI thread). Opt-in via -DSIGNALFORGE_BENCHMARKS=ON.
#include "video/color_correction.hpp"

#include <QImage>
#include <chrono>
#include <cstdint>
#include <cstdio>

int main() {
    using namespace signalforge::video;
    const int w = 1280;
    const int h = 720;
    QImage src(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        auto* p = src.scanLine(y);
        for (int x = 0; x < w; ++x) {
            p[x * 3 + 0] = static_cast<std::uint8_t>(x & 0xFF);
            p[x * 3 + 1] = static_cast<std::uint8_t>(y & 0xFF);
            p[x * 3 + 2] = static_cast<std::uint8_t>((x + y) & 0xFF);
        }
    }

    ColorParams params;  // a representative non-identity correction (saturation on)
    params.rGain = 1.2;
    params.gGain = 0.9;
    params.bGain = 1.1;
    params.brightness = 0.05;
    params.contrast = 1.1;
    params.gamma = 1.2;
    params.saturation = 1.3;
    ColorCorrector cc;
    cc.setParams(params);

    std::uint64_t sink = 0;
    sink += cc.apply(src).constScanLine(0)[0];  // warm-up

    const int n = 200;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        sink += cc.apply(src).constScanLine(0)[0];
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::printf(R"({"scenario":"color_correction_1280x720","frames":%d,"fps":%.1f,"ms_per_frame":%.3f,"sink":%llu})"
                "\n",
                n, n / secs, secs / n * 1000.0, static_cast<unsigned long long>(sink));
    return 0;
}
