// src/workbench/components/status_chip.hpp
#pragma once

#include <QFrame>
#include <QString>

class QLabel;

namespace signalforge::workbench {

/// A small status pill: a short label with a semantic `state` (e.g. "good",
/// "error", "live", "recording") exposed as a Qt property so `tokens.qss` can
/// colour it. Used in the top bar (connection / mode) and per-signal badges.
class StatusChip : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QString state READ state WRITE setState)

public:
    explicit StatusChip(QWidget* parent = nullptr);
    explicit StatusChip(const QString& text, const QString& state, QWidget* parent = nullptr);
    ~StatusChip() override;

    void setText(const QString& text);
    [[nodiscard]] QString text() const;

    /// Semantic state token; re-polishes so a QSS `[state="…"]` rule applies.
    void setState(const QString& state);
    [[nodiscard]] QString state() const {
        return state_;
    }

private:
    QLabel* label_ = nullptr;
    QString state_;
};

}  // namespace signalforge::workbench
