// src/dashboard/meter_view.cpp

#include "dashboard/meter_view.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace signalforge::dashboard {

namespace {
const QColor kFill(0x4f, 0xc3, 0xf7);  // matches PlotView's first palette colour
}

MeterView::MeterView(Style style, QWidget* parent) : QWidget(parent), style_(style) {
    setObjectName(style == Style::Bar ? QStringLiteral("barMeter") : QStringLiteral("gaugeMeter"));
    setMinimumSize(120, style == Style::Bar ? 64 : 110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

MeterView::~MeterView() = default;

void MeterView::setValue(double value, bool hasData) {
    value_ = value;
    hasData_ = hasData;
    update();
}

void MeterView::setRange(double lo, double hi) {
    lo_ = lo;
    hi_ = hi;
    update();
}

void MeterView::setUnit(const QString& unit) {
    unit_ = unit;
    update();
}

double MeterView::fraction() const {
    const double span = (hi_ - lo_ > 0.0) ? (hi_ - lo_) : 1.0;
    return std::clamp((value_ - lo_) / span, 0.0, 1.0);
}

void MeterView::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    p.fillRect(rect(), pal.color(QPalette::Base));
    const QColor track = pal.color(QPalette::Mid);
    const QColor text = pal.color(QPalette::Text);

    const QString valueText = hasData_ ? QString::number(value_, 'g', 4) : QStringLiteral("—");
    const QString unitSuffix = unit_.isEmpty() ? QString() : QStringLiteral(" ") + unit_;

    if (style_ == Style::Bar) {
        const QRectF barRect(12, height() / 2.0 - 12, width() - 24, 24);
        p.setPen(Qt::NoPen);
        p.setBrush(track);
        p.drawRoundedRect(barRect, 4, 4);
        if (hasData_) {
            QRectF fill = barRect;
            fill.setWidth(barRect.width() * fraction());
            p.setBrush(kFill);
            p.drawRoundedRect(fill, 4, 4);
        }
        p.setPen(text);
        p.drawText(QRectF(12, 4, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, valueText + unitSuffix);
        p.drawText(QRectF(12, barRect.bottom() + 2, (width() - 24) / 2, 16), Qt::AlignLeft,
                   QString::number(lo_, 'g', 3));
        p.drawText(QRectF(width() / 2.0, barRect.bottom() + 2, (width() - 24) / 2, 16), Qt::AlignRight,
                   QString::number(hi_, 'g', 3));
        return;
    }

    // Gauge: 180° arc (180°..0°) with a needle.
    const double margin = 14;
    const double radius = std::min(width() / 2.0 - margin, static_cast<double>(height()) - 2 * margin);
    const QPointF center(width() / 2.0, height() - margin);
    const QRectF arcRect(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);

    QPen arcPen(track, 8);
    arcPen.setCapStyle(Qt::FlatCap);
    p.setPen(arcPen);
    p.drawArc(arcRect, 0 * 16, 180 * 16);
    if (hasData_) {
        QPen valPen(kFill, 8);
        valPen.setCapStyle(Qt::FlatCap);
        p.setPen(valPen);
        p.drawArc(arcRect, 180 * 16, -static_cast<int>(180.0 * fraction()) * 16);

        const double angle = M_PI * (1.0 - fraction());  // 180°→0°
        const QPointF tip(center.x() + radius * 0.9 * std::cos(angle), center.y() - radius * 0.9 * std::sin(angle));
        p.setPen(QPen(text, 2));
        p.drawLine(center, tip);
    }
    p.setPen(text);
    p.drawText(QRectF(0, center.y() - 22, width(), 20), Qt::AlignHCenter | Qt::AlignBottom, valueText + unitSuffix);
}

}  // namespace signalforge::dashboard
