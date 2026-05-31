// src/workbench/components/inspector_panel.cpp

#include "workbench/components/inspector_panel.hpp"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace signalforge::workbench {

InspectorPanel::InspectorPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("inspectorPanel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // Header: swatch + title + subtitle.
    header_ = new QWidget(this);
    auto* headerRow = new QHBoxLayout(header_);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    swatch_ = new QFrame(header_);
    swatch_->setObjectName(QStringLiteral("inspectorSwatch"));
    swatch_->setFixedSize(12, 12);
    swatch_->setVisible(false);
    headerRow->addWidget(swatch_, 0, Qt::AlignTop);
    auto* titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(1);
    titleLabel_ = new QLabel(header_);
    titleLabel_->setProperty("class", QLatin1String("heading"));
    titleLabel_->setWordWrap(true);
    subtitleLabel_ = new QLabel(header_);
    subtitleLabel_->setProperty("class", QLatin1String("caption"));
    subtitleLabel_->setWordWrap(true);
    titleCol->addWidget(titleLabel_);
    titleCol->addWidget(subtitleLabel_);
    headerRow->addLayout(titleCol, 1);
    root->addWidget(header_);

    // Detail rows.
    rowsHost_ = new QWidget(this);
    rowsLayout_ = new QFormLayout(rowsHost_);
    rowsLayout_->setContentsMargins(0, 0, 0, 0);
    rowsLayout_->setHorizontalSpacing(10);
    rowsLayout_->setVerticalSpacing(4);
    rowsLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    rowsLayout_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    root->addWidget(rowsHost_);

    root->addStretch(1);

    // Placeholder shown when nothing is selected.
    placeholder_ = new QLabel(this);
    placeholder_->setObjectName(QStringLiteral("inspectorPlaceholder"));
    placeholder_->setProperty("class", QLatin1String("caption"));
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    root->addWidget(placeholder_);
    root->setStretchFactor(placeholder_, 2);

    showPlaceholder(tr("Select a signal or packet field to inspect."));
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::clearRows() {
    while (rowsLayout_->rowCount() > 0) {
        rowsLayout_->removeRow(0);  // deletes the label + field widgets
    }
    rowCount_ = 0;
}

void InspectorPanel::showDetails(const QString& title, const QString& subtitle, const QVector<Row>& rows,
                                 const QColor& accent) {
    clearRows();
    titleLabel_->setText(title);
    subtitleLabel_->setText(subtitle);
    subtitleLabel_->setVisible(!subtitle.isEmpty());

    if (accent.isValid()) {
        swatch_->setStyleSheet(QStringLiteral("background:%1; border-radius:2px;").arg(accent.name()));
        swatch_->setVisible(true);
    } else {
        swatch_->setVisible(false);
    }

    for (const Row& r : rows) {
        auto* value = new QLabel(r.second, rowsHost_);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);
        rowsLayout_->addRow(r.first, value);
    }
    rowCount_ = static_cast<int>(rows.size());

    header_->setVisible(true);
    rowsHost_->setVisible(true);
    placeholder_->setVisible(false);
    showingPlaceholder_ = false;
}

void InspectorPanel::showPlaceholder(const QString& message) {
    clearRows();
    titleLabel_->clear();
    subtitleLabel_->clear();
    swatch_->setVisible(false);
    header_->setVisible(false);
    rowsHost_->setVisible(false);
    placeholder_->setText(message);
    placeholder_->setVisible(true);
    showingPlaceholder_ = true;
}

QString InspectorPanel::titleText() const {
    return titleLabel_->text();
}

int InspectorPanel::rowCount() const {
    return rowCount_;
}

bool InspectorPanel::showingPlaceholder() const {
    return showingPlaceholder_;
}

}  // namespace signalforge::workbench
