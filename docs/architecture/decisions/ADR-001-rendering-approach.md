# ADR-001 — Rendering Approach for V1 Charts

**Status**: Accepted
**Date**: 2026-04-23
**Context**: M1 Qt Quick integration spike results

## Decision

Adopt QQuickWidget + Qt Quick Scene Graph for V1 chart rendering.

## Supporting Evidence

M1 spike (`docs/spikes/M1-qtquick-integration.md`) confirmed:

- Hardware-accelerated OpenGL via AMD radeonsi / Mesa 25.2.8 is functional on the development host (AMD Cezanne iGPU).
- QQuickWidget integration with QDockWidget: floating, re-docking, context-menu propagation, and hide/show lifecycle all behave correctly (Checks 1, 3, 4 all Pass or benign-Mixed).
- Check 2 (HiDPI) visual verification by human, 2026-04-23: functional; default window size renders smoothly at all four tested scales (125% / 150% / 175% / 200%); full-screen maximized exhibits mild lag, indicative of GPU fill-rate pressure at large pixel counts on the iGPU.
- Check 5 (multi-instance GPU) produced the binding constraint for V1 scope: per-widget GPU utilization ~15.9% on this iGPU, implying the original "20 concurrent charts" target is not achievable on this hardware class. Linear extrapolation is unreliable because M6's Scene Graph architecture is qualitatively different from Check 5's independent QQuickWidget contexts, but the direction — GPU saturation precedes CPU saturation — is load-bearing for V1 scope.
- Check 5 CPU figure exceeding the "< 30% single-core" spec threshold is a spec-language artifact (multi-core observation measured against single-core threshold), not a hardware signal. Per-widget CPU ~16.2% of one core is readily within budget.

## Consequences

Performance targets in `architecture.md §8.4` are revised to reflect measurement rather than aspiration:

| Metric | Prior target | Revised target | Rationale |
|---|---|---|---|
| Concurrent signals online | ≥ 100 | **≥ 60** | GPU density, not algorithmic |
| Concurrent real-time chart widgets | ≥ 20 | **≥ 8–12** | Check 5 density extrapolation |
| Aggregate input rate | ≥ 10 k points/s | unchanged | Not GPU-bound |
| Per-signal rate | ≥ 1 kHz | unchanged | Not GPU-bound |
| End-to-end latency P50 / P99 | ≤ 80 / 200 ms | unchanged | Not chart-density-bound |
| UI frame rate | ≥ 30 FPS sustained | unchanged | Chart-local target |

Additional operational notes:

1. M6 will benchmark at 10 concurrent charts as the starting point and scale up until density-driven degradation appears. Final supported density is set from measurement, not from the prior aspirational target.

2. M6 will also benchmark a "maximized single chart" scenario, because the human-observed mild lag at full-screen indicates fill-rate pressure that is not captured by the density benchmark alone.

3. `QWindow::createWindowContainer` remains available as a latent fallback. M6 rendering code will be structured such that swapping to QWindow is possible, but the swap is not planned.

4. `QPainter + OpenGL` self-render is rejected. The cost of losing Scene Graph downsampling infrastructure is larger than the GPU headroom a software-rasterized chart would free.

## Considered Alternatives

- **QWindow container**: shares QQuickWidget's OpenGL context; does not address GPU saturation. Rejected.
- **QPainter + OpenGL self-render**: avoids GPU saturation at the cost of rewriting all chart rendering and giving up Qt Quick animation and interaction ecosystem. Rejected.

## Open Items

- Per-process VRAM attribution on AMD shared-VRAM iGPU: no free-tier tool provides it. M6 will benchmark with system-wide VRAM deltas during controlled workload windows and accept this as adequate for V1.

## Revisit Trigger

If M6 benchmarking shows GPU saturation below 8 concurrent charts at the target input rate, a new ADR will re-evaluate. The fallback candidate if re-evaluated: QWindow container with explicit context sharing, then QPainter + OpenGL as last resort.
