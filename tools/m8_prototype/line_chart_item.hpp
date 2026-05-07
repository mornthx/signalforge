// tools/m8_prototype/line_chart_item.hpp
//
// Minimal QQuickItem subclass that renders a polyline of (x,y) doubles
// using Qt's Scene Graph. No QML, no styling — just enough geometry
// to measure rendering cost honestly.

#pragma once

#include <QQuickItem>
#include <QVector>

class LineChartItem : public QQuickItem {
    Q_OBJECT
public:
    explicit LineChartItem(QQuickItem* parent = nullptr);

    void setPoints(QVector<QPointF> points);
    void setRanges(double xMin, double xMax, double yMin, double yMax);
    void setColor(QColor c) { color_ = c; }

    void appendPoint(QPointF p);

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override;

private:
    QVector<QPointF> points_;
    double xMin_ = 0.0;
    double xMax_ = 1.0;
    double yMin_ = -1.0;
    double yMax_ = 1.0;
    QColor color_{Qt::cyan};
    bool dirty_ = true;
};
