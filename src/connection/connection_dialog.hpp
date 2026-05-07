// src/connection/connection_dialog.hpp
#pragma once

#include "connection/connection.hpp"

#include <QDialog>
#include <QStringList>

class QStackedWidget;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QListWidget;

namespace signalforge::connection {

/// Modal `QDialog` for adding / editing one connection (per
/// design decision M9.1 + M9.3). The dialog hosts a driver-type
/// combo that drives a `QStackedWidget` so each driver type
/// shows only its own fields plus the common header.
///
/// Per-driver validation runs continuously; the OK button is
/// disabled until the inputs form a valid `ConnectionConfig`.
///
/// S1 declares the public surface; S5 implements the widgets,
/// validation, and getter.
class ConnectionDialog : public QDialog {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionDialog)

public:
    /// `availableSchemaIds` populates the decoder schema combo.
    /// Caller (typically MainWindow) is responsible for
    /// enumerating schemas (e.g., via filesystem walk of
    /// `examples/schemas/*.yaml`); this dialog does not query
    /// `DecoderRegistrar` directly to keep M5's frozen surface
    /// untouched.
    explicit ConnectionDialog(QStringList availableSchemaIds, QWidget* parent = nullptr);
    ~ConnectionDialog() override;

    /// Pre-fill the dialog with an existing connection's config.
    /// Used by edit-mode invocations.
    void setConfig(const ConnectionConfig& config);

    /// Read the current dialog state as a `ConnectionConfig`.
    /// Only valid when `isValid()` is `true`.
    [[nodiscard]] ConnectionConfig config() const;

    /// Whether the current dialog state forms a valid config.
    [[nodiscard]] bool isValid() const;

private:
    QStringList availableSchemaIds_;
};

}  // namespace signalforge::connection
