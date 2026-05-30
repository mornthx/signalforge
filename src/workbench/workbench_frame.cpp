// src/workbench/workbench_frame.cpp

#include "workbench/workbench_frame.hpp"

#include "workbench/components/activity_rail.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace signalforge::workbench {

WorkbenchFrame::WorkbenchFrame(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("workbenchFrame"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ----- Top bar -----------------------------------------------------------
    topBar_ = new QFrame(this);
    topBar_->setObjectName(QStringLiteral("workbenchTopBar"));
    topBarLayout_ = new QHBoxLayout(topBar_);
    topBarLayout_->setContentsMargins(8, 4, 8, 4);
    topBarLayout_->setSpacing(8);
    titleLabel_ = new QLabel(topBar_);
    titleLabel_->setObjectName(QStringLiteral("workbenchTitle"));
    titleLabel_->setProperty("class", QLatin1String("heading"));
    topBarLayout_->addWidget(titleLabel_);
    topBarLayout_->addStretch(1);  // trailing widgets insert after this
    root->addWidget(topBar_);

    // ----- Body: vertical splitter [ middle | drawer ] -----------------------
    verticalSplitter_ = new QSplitter(Qt::Vertical, this);
    verticalSplitter_->setObjectName(QStringLiteral("workbenchVerticalSplitter"));
    verticalSplitter_->setChildrenCollapsible(false);

    // Middle band: rail (fixed) + horizontal splitter [ content | inspector ].
    auto* middle = new QWidget(verticalSplitter_);
    auto* middleLayout = new QHBoxLayout(middle);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);

    rail_ = new ActivityRail(middle);
    middleLayout->addWidget(rail_);

    contentSplitter_ = new QSplitter(Qt::Horizontal, middle);
    contentSplitter_->setObjectName(QStringLiteral("workbenchContentSplitter"));
    contentSplitter_->setChildrenCollapsible(false);

    contentStack_ = new QStackedWidget(contentSplitter_);
    contentStack_->setObjectName(QStringLiteral("workbenchContent"));
    contentSplitter_->addWidget(contentStack_);

    inspectorFrame_ = new QFrame(contentSplitter_);
    inspectorFrame_->setObjectName(QStringLiteral("workbenchInspector"));
    inspectorLayout_ = new QHBoxLayout(inspectorFrame_);
    inspectorLayout_->setContentsMargins(0, 0, 0, 0);
    inspectorLayout_->setSpacing(0);
    contentSplitter_->addWidget(inspectorFrame_);
    contentSplitter_->setStretchFactor(0, 1);  // content grows, inspector stays
    contentSplitter_->setStretchFactor(1, 0);
    inspectorFrame_->hide();  // hidden until content is supplied + shown

    middleLayout->addWidget(contentSplitter_, 1);
    verticalSplitter_->addWidget(middle);

    drawerFrame_ = new QFrame(verticalSplitter_);
    drawerFrame_->setObjectName(QStringLiteral("workbenchDrawer"));
    drawerLayout_ = new QHBoxLayout(drawerFrame_);
    drawerLayout_->setContentsMargins(0, 0, 0, 0);
    drawerLayout_->setSpacing(0);
    verticalSplitter_->addWidget(drawerFrame_);
    verticalSplitter_->setStretchFactor(0, 1);  // middle grows, drawer stays
    verticalSplitter_->setStretchFactor(1, 0);
    drawerFrame_->hide();

    root->addWidget(verticalSplitter_, 1);

    connect(rail_, &ActivityRail::modeSelected, this, [this](const QString& id) {
        if (auto* page = pages_.value(id, nullptr)) {
            contentStack_->setCurrentWidget(page);
        }
        currentMode_ = id;
        Q_EMIT modeChanged(id);
    });
}

WorkbenchFrame::~WorkbenchFrame() = default;

void WorkbenchFrame::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void WorkbenchFrame::addTopBarWidget(QWidget* widget) {
    if (widget != nullptr) {
        topBarLayout_->addWidget(widget);
    }
}

void WorkbenchFrame::addMode(const QString& id, const QString& label, QWidget* content, const QString& glyph) {
    if (pages_.contains(id)) {
        return;  // ids must be unique; ignore duplicates
    }
    auto* page = content != nullptr ? content : new QWidget(this);
    contentStack_->addWidget(page);
    pages_.insert(id, page);
    rail_->addMode(id, label, glyph);

    if (pages_.size() == 1) {
        contentStack_->setCurrentWidget(page);
        currentMode_ = id;
    }
}

void WorkbenchFrame::setCurrentMode(const QString& id) {
    if (auto* page = pages_.value(id, nullptr)) {
        rail_->setCurrentMode(id);
        contentStack_->setCurrentWidget(page);
        currentMode_ = id;
    }
}

void WorkbenchFrame::setSlotContent(QFrame* slot, QHBoxLayout* slotLayout, QWidget*& held, QWidget* widget) {
    if (held != nullptr) {
        slotLayout->removeWidget(held);
        held->setParent(nullptr);
        held->deleteLater();
        held = nullptr;
    }
    if (widget != nullptr) {
        widget->setParent(slot);
        slotLayout->addWidget(widget);
        held = widget;
    }
}

void WorkbenchFrame::setInspector(QWidget* widget) {
    setSlotContent(inspectorFrame_, inspectorLayout_, inspectorContent_, widget);
}

void WorkbenchFrame::setInspectorVisible(bool visible) {
    inspectorFrame_->setVisible(visible);
}

bool WorkbenchFrame::isInspectorVisible() const {
    return inspectorFrame_->isVisibleTo(const_cast<WorkbenchFrame*>(this));
}

void WorkbenchFrame::setDrawer(QWidget* widget) {
    setSlotContent(drawerFrame_, drawerLayout_, drawerContent_, widget);
}

void WorkbenchFrame::setDrawerVisible(bool visible) {
    drawerFrame_->setVisible(visible);
}

bool WorkbenchFrame::isDrawerVisible() const {
    return drawerFrame_->isVisibleTo(const_cast<WorkbenchFrame*>(this));
}

}  // namespace signalforge::workbench
