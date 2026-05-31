// src/workbench/components/inspector_panel.cpp

#include "workbench/components/inspector_panel.hpp"

#include "workbench/components/flow_layout.hpp"

#include <QAbstractButton>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
    // Close (×) button — dismisses the inspector sidebar.
    auto* closeBtn = new QPushButton(QStringLiteral("✕"), header_);
    closeBtn->setObjectName(QStringLiteral("inspectorClose"));
    closeBtn->setFlat(true);
    closeBtn->setFixedSize(20, 20);
    closeBtn->setToolTip(tr("Hide the inspector"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &InspectorPanel::closeRequested);
    headerRow->addWidget(closeBtn, 0, Qt::AlignTop);
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

    // Custom editable body (e.g. a dashboard panel's property form); empty until
    // a caller injects one via setContent().
    contentHost_ = new QWidget(this);
    contentLayout_ = new QVBoxLayout(contentHost_);
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(6);
    contentHost_->setVisible(false);
    root->addWidget(contentHost_);

    // Action buttons (set colour, add to dashboard, …) in a wrapping flow so
    // they use vertical space instead of being crammed onto one line; host
    // hidden when empty.
    actionsHost_ = new QWidget(this);
    actionsLayout_ = new FlowLayout(actionsHost_, /*margin=*/0, /*hSpacing=*/6, /*vSpacing=*/6);
    actionsHost_->setVisible(false);
    root->addWidget(actionsHost_);

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

void InspectorPanel::clearActions() {
    // Delete synchronously (not deleteLater) so accessors reflect the cleared
    // state immediately. Deleting a button removes it from the layout; any
    // leftover spacer items are then drained.
    const QList<QAbstractButton*> buttons = actionsHost_->findChildren<QAbstractButton*>();
    for (QAbstractButton* b : buttons) {
        delete b;
    }
    while (QLayoutItem* item = actionsLayout_->takeAt(0)) {
        delete item;
    }
    actionsHost_->setVisible(false);
}

void InspectorPanel::setContent(QWidget* body) {
    if (content_ != nullptr) {
        contentLayout_->removeWidget(content_);
        content_->deleteLater();
        content_ = nullptr;
    }
    content_ = body;
    if (content_ != nullptr) {
        content_->setParent(contentHost_);
        contentLayout_->addWidget(content_);
        content_->show();
    }
    contentHost_->setVisible(content_ != nullptr);
}

bool InspectorPanel::hasContent() const {
    return content_ != nullptr;
}

void InspectorPanel::setActions(const QVector<Action>& actions) {
    clearActions();
    for (const Action& a : actions) {
        auto* button = new QPushButton(a.first, actionsHost_);
        button->setProperty("class", QLatin1String("inspectorAction"));
        const std::function<void()> cb = a.second;
        connect(button, &QPushButton::clicked, this, [cb]() {
            if (cb) {
                cb();
            }
        });
        actionsLayout_->addWidget(button);
    }
    actionsHost_->setVisible(!actions.isEmpty());
}

int InspectorPanel::actionCount() const {
    return static_cast<int>(actionsHost_->findChildren<QAbstractButton*>().size());
}

QAbstractButton* InspectorPanel::actionButton(const QString& label) const {
    const QList<QAbstractButton*> buttons = actionsHost_->findChildren<QAbstractButton*>();
    for (QAbstractButton* b : buttons) {
        if (b->text() == label) {
            return b;
        }
    }
    return nullptr;
}

void InspectorPanel::showDetails(const QString& title, const QString& subtitle, const QVector<Row>& rows,
                                 const QColor& accent) {
    clearRows();
    setContent(nullptr);  // a fresh detail view drops any prior custom body
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
    clearActions();
    setContent(nullptr);
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
