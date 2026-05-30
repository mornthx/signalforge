// src/dashboard/panel_config_dialog.cpp

#include "dashboard/panel_config_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QVBoxLayout>
#include <array>

namespace signalforge::dashboard {

namespace {

struct TypeChoice {
    PanelType type;
    const char* label;
};
constexpr std::array<TypeChoice, 6> kTypes{{{PanelType::Numeric, "Numeric"},
                                            {PanelType::State, "State"},
                                            {PanelType::Plot, "Plot"},
                                            {PanelType::Bar, "Bar"},
                                            {PanelType::Gauge, "Gauge"},
                                            {PanelType::Table, "Table"}}};

bool isSingleSignalType(PanelType t) {
    return t == PanelType::Numeric || t == PanelType::State || t == PanelType::Bar || t == PanelType::Gauge;
}

}  // namespace

PanelConfigDialog::PanelConfigDialog(const PanelConfig& initial, const QStringList& availableSignals, QWidget* parent)
    : QDialog(parent), initial_(initial) {
    setWindowTitle(tr("Configure panel"));
    setModal(true);

    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    typeCombo_ = new QComboBox(this);
    for (const auto& tc : kTypes) {
        typeCombo_->addItem(tr(tc.label), static_cast<int>(tc.type));
        if (tc.type == initial.type) {
            typeCombo_->setCurrentIndex(typeCombo_->count() - 1);
        }
    }
    form->addRow(tr("Show as"), typeCombo_);

    titleEdit_ = new QLineEdit(initial.title, this);
    titleEdit_->setPlaceholderText(tr("(derived from signal)"));
    form->addRow(tr("Title"), titleEdit_);

    signalList_ = new QListWidget(this);
    signalList_->setSelectionMode(QAbstractItemView::NoSelection);
    for (const QString& sid : availableSignals) {
        auto* item = new QListWidgetItem(sid, signalList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(initial.signalIds.contains(sid) ? Qt::Checked : Qt::Unchecked);
    }
    form->addRow(tr("Signals"), signalList_);

    limitRangeCheck_ = new QCheckBox(tr("Limit value range"), this);
    minSpin_ = new QDoubleSpinBox(this);
    maxSpin_ = new QDoubleSpinBox(this);
    minSpin_->setRange(-1e12, 1e12);
    maxSpin_->setRange(-1e12, 1e12);
    const bool hasRange = initial.rangeMin.has_value() && initial.rangeMax.has_value();
    limitRangeCheck_->setChecked(hasRange);
    minSpin_->setValue(initial.rangeMin.value_or(0.0));
    maxSpin_->setValue(initial.rangeMax.value_or(100.0));
    minSpin_->setEnabled(hasRange);
    maxSpin_->setEnabled(hasRange);
    connect(limitRangeCheck_, &QCheckBox::toggled, minSpin_, &QWidget::setEnabled);
    connect(limitRangeCheck_, &QCheckBox::toggled, maxSpin_, &QWidget::setEnabled);
    form->addRow(QString(), limitRangeCheck_);
    form->addRow(tr("Min"), minSpin_);
    form->addRow(tr("Max"), maxSpin_);

    unitEdit_ = new QLineEdit(initial.unitOverride, this);
    unitEdit_->setPlaceholderText(tr("(use signal unit)"));
    form->addRow(tr("Unit override"), unitEdit_);

    decimalsSpin_ = new QSpinBox(this);
    decimalsSpin_->setRange(0, 10);
    decimalsSpin_->setValue(initial.decimals);
    form->addRow(tr("Decimals"), decimalsSpin_);

    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

PanelConfigDialog::~PanelConfigDialog() = default;

PanelConfig PanelConfigDialog::result() const {
    PanelConfig cfg = initial_;  // preserve id + geometry
    cfg.type = static_cast<PanelType>(typeCombo_->currentData().toInt());
    cfg.title = titleEdit_->text();

    QStringList chosen;
    for (int i = 0; i < signalList_->count(); ++i) {
        if (signalList_->item(i)->checkState() == Qt::Checked) {
            chosen << signalList_->item(i)->text();
        }
    }
    if (isSingleSignalType(cfg.type) && chosen.size() > 1) {
        chosen = {chosen.first()};
    }
    cfg.signalIds = chosen;

    if (limitRangeCheck_->isChecked()) {
        cfg.rangeMin = minSpin_->value();
        cfg.rangeMax = maxSpin_->value();
    } else {
        cfg.rangeMin.reset();
        cfg.rangeMax.reset();
    }
    cfg.unitOverride = unitEdit_->text();
    cfg.decimals = decimalsSpin_->value();
    return cfg;
}

}  // namespace signalforge::dashboard
