// src/dashboard/meter_view.hpp
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

namespace signalforge::dashboard {

/// Shared renderer for the Bar and Gauge panels: draws a single scalar
/// against a [min, max] range, as either a horizontal bar or a 180° arc
/// gauge with a needle. Theme-aware QPainter; headless-capturable.
class MeterView : public QWidget {
    Q_OBJECT

public:
    enum class Style { Bar, Gauge };

    explicit MeterView(Style style, QWidget* parent = nullptr);
    ~MeterView() override;

    /// Set the current value and whether data is available ("—" if not).
    void setValue(double value, bool hasData);
    /// Set the display range [lo, hi].
    void setRange(double lo, double hi);
    /// Unit suffix shown next to the value.
    void setUnit(const QString& unit);
    /// Fill / arc colour (the bound signal's identity colour). Defaults to the
    /// shared accent if never set.
    void setColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] double fraction() const;

    Style style_;
    double value_ = 0.0;
    bool hasData_ = false;
    double lo_ = 0.0;
    double hi_ = 1.0;
    QString unit_;
    QColor fill_{0x4f, 0xc3, 0xf7};  ///< default matches PlotView's first palette colour
};

}  // namespace signalforge::dashboard
