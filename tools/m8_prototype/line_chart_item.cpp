// tools/m8_prototype/line_chart_item.cpp
#include "line_chart_item.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGNode>

LineChartItem::LineChartItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

void LineChartItem::setPoints(QVector<QPointF> points) {
    points_ = std::move(points);
    dirty_ = true;
    update();
}

void LineChartItem::setRanges(double xMin, double xMax, double yMin, double yMax) {
    xMin_ = xMin;
    xMax_ = xMax;
    yMin_ = yMin;
    yMax_ = yMax;
    dirty_ = true;
    update();
}

void LineChartItem::appendPoint(QPointF p) {
    points_.append(p);
    dirty_ = true;
    update();
}

QSGNode* LineChartItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geom->setLineWidth(1);
        geom->setDrawingMode(QSGGeometry::DrawLineStrip);
        node->setGeometry(geom);
        node->setFlag(QSGNode::OwnsGeometry);
        auto* mat = new QSGFlatColorMaterial;
        mat->setColor(color_);
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
        dirty_ = true;
    } else {
        // Refresh material color in case it changed since last node creation.
        auto* mat = static_cast<QSGFlatColorMaterial*>(node->material());
        if (mat->color() != color_) {
            mat->setColor(color_);
            node->markDirty(QSGNode::DirtyMaterial);
        }
    }

    // Always re-write the vertex buffer. For benchmarking, the cost of
    // pure render (Scenario 1) requires the SG to actually composite +
    // swap each frame; otherwise Qt's "nothing changed" optimization
    // skips frames and frameSwapped never fires. For Scenarios 2/3/4,
    // data really is changing per-frame, so this matches reality.
    QSGGeometry* g = node->geometry();
    const int n = points_.size();
    g->allocate(n);
    if (n > 0) {
        QSGGeometry::Point2D* v = g->vertexDataAsPoint2D();
        const double w = static_cast<double>(width());
        const double h = static_cast<double>(height());
        const double xSpan = xMax_ - xMin_;
        const double ySpan = yMax_ - yMin_;
        const double xScale = (xSpan > 0.0 ? (w / xSpan) : 0.0);
        const double yScale = (ySpan > 0.0 ? (h / ySpan) : 0.0);
        for (int i = 0; i < n; ++i) {
            const QPointF& p = points_[i];
            const float x = static_cast<float>((p.x() - xMin_) * xScale);
            const float y = static_cast<float>(h - (p.y() - yMin_) * yScale);
            v[i].set(x, y);
        }
    }
    node->markDirty(QSGNode::DirtyGeometry);
    dirty_ = false;
    return node;
}
