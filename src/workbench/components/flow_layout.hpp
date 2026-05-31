// src/workbench/components/flow_layout.hpp
#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QSize>
#include <QStyle>

namespace signalforge::workbench {

/// A layout that arranges its items left-to-right and wraps to the next line
/// when the width runs out — so a row of buttons uses vertical space instead of
/// being crammed onto one line. Adapted from Qt's canonical FlowLayout example.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent, int margin = 0, int hSpacing = -1, int vSpacing = -1);
    explicit FlowLayout(int margin = 0, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    FlowLayout(const FlowLayout&) = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int horizontalSpacing() const;
    [[nodiscard]] int verticalSpacing() const;
    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    [[nodiscard]] QLayoutItem* takeAt(int index) override;
    [[nodiscard]] QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    [[nodiscard]] QSize sizeHint() const override;

private:
    [[nodiscard]] int doLayout(const QRect& rect, bool testOnly) const;
    [[nodiscard]] int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> itemList_;
    int hSpace_;
    int vSpace_;
};

}  // namespace signalforge::workbench
