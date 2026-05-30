// src/dashboard/plot_view.cpp

#include "dashboard/plot_view.hpp"

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "chart/time_axis_manager.hpp"
#include "dashboard/value_format.hpp"
#include "decode/decoder_interface.hpp"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace signalforge::dashboard {

namespace {

// Distinct saturated colours legible on both dark and light themes.
const QColor kPalette[] = {
    QColor(0x4f, 0xc3, 0xf7), QColor(0xff, 0xb7, 0x4d), QColor(0x81, 0xc7, 0x84), QColor(0xe5, 0x73, 0x73),
    QColor(0xba, 0x68, 0xc8), QColor(0x4d, 0xb6, 0xac), QColor(0xff, 0xd5, 0x4f), QColor(0xa1, 0x88, 0x7f),
};
constexpr int kPaletteSize = static_cast<int>(sizeof(kPalette) / sizeof(kPalette[0]));

constexpr int kMarginLeft = 56;
constexpr int kMarginBottom = 24;
constexpr int kMarginTop = 22;
constexpr int kMarginRight = 10;
constexpr int kYTicks = 4;
constexpr int kXTicks = 6;

QString fieldName(const QString& signalId) {
    const int slash = signalId.indexOf(QLatin1Char('/'));
    return slash >= 0 ? signalId.mid(slash + 1) : signalId;
}

}  // namespace

PlotView::PlotView(signalforge::buffer::SignalBufferRegistry& registry, signalforge::chart::TimeAxisManager& timeAxis,
                   QWidget* parent)
    : QWidget(parent), registry_(&registry), timeAxis_(&timeAxis) {
    setObjectName(QStringLiteral("plotView"));
    setMinimumSize(120, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(timeAxis_, &signalforge::chart::TimeAxisManager::rangeChanged, this, &PlotView::refresh);
}

PlotView::~PlotView() = default;

int PlotView::indexOf(const QString& signalId) const {
    for (int i = 0; i < static_cast<int>(series_.size()); ++i) {
        if (series_[i].id == signalId) {
            return i;
        }
    }
    return -1;
}

void PlotView::addSignal(const QString& signalId) {
    if (signalId.isEmpty() || indexOf(signalId) >= 0) {
        return;
    }
    Series s;
    s.id = signalId;
    s.color = kPalette[(nextColorIndex_++ % kPaletteSize + kPaletteSize) % kPaletteSize];
    series_.push_back(std::move(s));
    refresh();
}

void PlotView::removeSignal(const QString& signalId) {
    const int idx = indexOf(signalId);
    if (idx >= 0) {
        series_.erase(series_.begin() + idx);
        refresh();
    }
}

QStringList PlotView::signalIds() const {
    QStringList out;
    out.reserve(static_cast<int>(series_.size()));
    for (const auto& s : series_) {
        out.append(s.id);
    }
    return out;
}

bool PlotView::hasSignal(const QString& signalId) const {
    return indexOf(signalId) >= 0;
}

void PlotView::setPerSignalNormalize(bool on) {
    if (perSignalNormalize_ != on) {
        perSignalNormalize_ = on;
        refresh();
    }
}

void PlotView::setYRange(std::optional<double> min, std::optional<double> max) {
    explicitMin_ = min;
    explicitMax_ = max;
    refresh();
}

QColor PlotView::colorFor(const QString& signalId) const {
    const int idx = indexOf(signalId);
    return idx >= 0 ? series_[idx].color : QColor();
}

bool PlotView::hasData() const {
    return std::any_of(series_.begin(), series_.end(), [](const Series& s) { return !s.samples.empty(); });
}

void PlotView::recomputeRanges() {
    const auto start = timeAxis_->visibleStart();
    const auto end = timeAxis_->visibleEnd();
    const auto windowDur = end - start;
    const auto targetPx = static_cast<std::size_t>(std::max(1, width() - kMarginLeft - kMarginRight));

    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    std::chrono::steady_clock::time_point newest{};
    bool anyData = false;
    for (auto& s : series_) {
        s.samples.clear();
        s.seriesMin = std::numeric_limits<double>::infinity();
        s.seriesMax = -std::numeric_limits<double>::infinity();
        auto* buf = registry_->bufferFor(s.id);
        if (buf == nullptr) {
            continue;
        }
        s.samples = buf->queryRange(start, end, targetPx);
        if (s.samples.empty()) {
            // The LOD-decimated path can return nothing for sparse/recent
            // data even when in-range samples exist; fall back to the
            // undecimated query (bounded — sparse data is few samples).
            s.samples = buf->queryRange(start, end, 0);
        }
        if (!s.samples.empty()) {
            anyData = true;
            newest = std::max(newest, s.samples.back().timestamp);
        }
        for (const auto& sample : s.samples) {
            const double v = valueToDouble(sample.value);
            if (!std::isfinite(v)) {
                continue;
            }
            s.seriesMin = std::min(s.seriesMin, v);
            s.seriesMax = std::max(s.seriesMax, v);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }

    // Anchor the paint window's right edge to the newest available sample
    // rather than "now": readers see data only up to the last publish (the
    // buffer's publish cadence/flush lag), so mapping against "now" leaves a
    // growing empty gap on the right (M28 problem A). Following the data keeps
    // the newest trace point at the right edge; any clip lands on the (older)
    // left edge. Capture the window so paintEvent maps against the same bounds.
    queryEnd_ = anyData ? newest : end;
    queryStart_ = queryEnd_ - windowDur;

    if (explicitMin_.has_value()) {
        lo = *explicitMin_;
    }
    if (explicitMax_.has_value()) {
        hi = *explicitMax_;
    }
    if (!std::isfinite(lo) || !std::isfinite(hi)) {
        lo = -1.0;
        hi = 1.0;
    } else if (lo == hi) {
        lo -= 0.5;
        hi += 0.5;
    } else {
        const double pad = 0.05 * (hi - lo);
        if (!explicitMin_.has_value()) {
            lo -= pad;
        }
        if (!explicitMax_.has_value()) {
            hi += pad;
        }
    }
    yMin_ = lo;
    yMax_ = hi;
}

void PlotView::refresh() {
    recomputeRanges();
    update();
}

void PlotView::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = palette();
    p.fillRect(rect(), pal.color(QPalette::Base));

    const QRectF plot(kMarginLeft, kMarginTop, std::max(1, width() - kMarginLeft - kMarginRight),
                      std::max(1, height() - kMarginTop - kMarginBottom));
    const double plotW = plot.width();
    const double plotH = plot.height();

    const QColor gridColor = pal.color(QPalette::Mid);
    const QColor textColor = pal.color(QPalette::Text);
    const QColor borderColor = pal.color(QPalette::Shadow);

    // Determine shared unit (shown on Y axis when not normalized and all
    // signals agree on a unit).
    QString sharedUnit;
    bool unitConsistent = true;
    for (const auto& s : series_) {
        if (auto* buf = registry_->bufferFor(s.id); buf != nullptr) {
            const QString u = buf->metadata().unit;
            if (sharedUnit.isEmpty()) {
                sharedUnit = u;
            } else if (!u.isEmpty() && u != sharedUnit) {
                unitConsistent = false;
            }
        }
    }

    // Grid + Y axis labels.
    p.setPen(QPen(gridColor, 1));
    QFont small = p.font();
    small.setPointSizeF(std::max(7.0, small.pointSizeF() - 1.0));
    p.setFont(small);
    const QFontMetrics fm(small);
    for (int i = 0; i <= kYTicks; ++i) {
        const double y = plot.bottom() - (plotH * i) / kYTicks;
        p.setPen(QPen(gridColor, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        QString label;
        if (perSignalNormalize_) {
            label = QString::number(static_cast<double>(i) / kYTicks, 'f', 2);
        } else {
            const double v = yMin_ + (yMax_ - yMin_) * (static_cast<double>(i) / kYTicks);
            label = QString::number(v, 'g', 4);
        }
        p.setPen(textColor);
        p.drawText(QRectF(0, y - 8, kMarginLeft - 4, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // X axis time labels (relative seconds, 0 = now at the right edge).
    // Use the window captured by the last recompute(), NOT a fresh read of
    // the (live, now()-tracking) axis — otherwise the query-time and
    // paint-time windows differ and the oldest samples map to negative
    // offsets, drifting the trace left onto the Y axis (M27 problem 1).
    const auto durNs = std::chrono::duration_cast<std::chrono::nanoseconds>(queryEnd_ - queryStart_).count();
    const double durSec = static_cast<double>(durNs) / 1e9;
    for (int i = 0; i <= kXTicks; ++i) {
        const double x = plot.left() + (plotW * i) / kXTicks;
        p.setPen(QPen(gridColor, 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        const double tRel = -durSec * (1.0 - static_cast<double>(i) / kXTicks);
        const QString label = (i == kXTicks) ? QStringLiteral("0") : QStringLiteral("%1s").arg(tRel, 0, 'g', 2);
        p.setPen(textColor);
        p.drawText(QRectF(x - 24, plot.bottom() + 2, 48, kMarginBottom - 2), Qt::AlignHCenter | Qt::AlignTop, label);
    }

    // Y unit caption (top-left of plot) when meaningful.
    if (!perSignalNormalize_ && unitConsistent && !sharedUnit.isEmpty()) {
        p.setPen(textColor);
        p.drawText(QRectF(2, 2, kMarginLeft + 40, 16), Qt::AlignLeft | Qt::AlignVCenter, sharedUnit);
    }

    // Plot border.
    p.setPen(QPen(borderColor, 1));
    p.drawRect(plot);

    const auto axisStart = queryStart_;
    const double xScale = (durNs > 0) ? (plotW / static_cast<double>(durNs)) : 0.0;
    const double yspan = (yMax_ - yMin_ > 0.0) ? (yMax_ - yMin_) : 1.0;

    // Clip series + cursor to the plot rect so nothing spills onto the
    // axes/margins even at the window edges (M27 problem 1).
    p.save();
    p.setClipRect(plot);

    // Series polylines.
    for (const auto& s : series_) {
        if (s.samples.empty()) {
            continue;
        }
        const bool norm = perSignalNormalize_;
        const double sLo = s.seriesMin;
        const double sSpan = (s.seriesMax - s.seriesMin > 0.0) ? (s.seriesMax - s.seriesMin) : 1.0;
        QPolygonF poly;
        poly.reserve(static_cast<int>(s.samples.size()));
        for (const auto& sample : s.samples) {
            const double v = valueToDouble(sample.value);
            if (!std::isfinite(v)) {
                continue;
            }
            const auto offsetNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(sample.timestamp - axisStart).count();
            const double x = plot.left() + static_cast<double>(offsetNs) * xScale;
            const double frac = norm ? (v - sLo) / sSpan : (v - yMin_) / yspan;
            const double y = plot.bottom() - std::clamp(frac, 0.0, 1.0) * plotH;
            poly.append(QPointF(x, y));
        }
        p.setPen(QPen(s.color, 2));
        p.drawPolyline(poly);
    }

    // Live "now" cursor at the captured window end (not a fresh now()).
    {
        const auto nowOff = std::chrono::duration_cast<std::chrono::nanoseconds>(queryEnd_ - axisStart).count();
        const double cx = std::clamp(plot.left() + static_cast<double>(nowOff) * xScale, plot.left(), plot.right());
        QColor cursor = textColor;
        cursor.setAlpha(120);
        p.setPen(QPen(cursor, 1, Qt::DashLine));
        p.drawLine(QPointF(cx, plot.top()), QPointF(cx, plot.bottom()));
    }
    p.restore();  // end plot-rect clip (legend/labels draw unclipped)

    // Legend (top strip).
    double lx = plot.left();
    const double ly = 4;
    p.setFont(small);
    for (const auto& s : series_) {
        p.fillRect(QRectF(lx, ly + 3, 10, 10), s.color);
        const QString name = fieldName(s.id);
        const int textW = fm.horizontalAdvance(name) + 6;
        p.setPen(textColor);
        p.drawText(QRectF(lx + 14, ly, textW, 16), Qt::AlignLeft | Qt::AlignVCenter, name);
        lx += 14 + textW + 12;
        if (lx > plot.right() - 40) {
            break;
        }
    }
}

}  // namespace signalforge::dashboard
