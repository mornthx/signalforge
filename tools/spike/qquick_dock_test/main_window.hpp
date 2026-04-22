#pragma once

#include <QDockWidget>
#include <QMainWindow>
#include <QString>
#include <QtQuickWidgets/QQuickWidget>
#include <array>

namespace signalforge::spike {

/// Spike main window hosting three QQuickWidget instances in three QDockWidgets.
/// Each dock renders the same QML scene (DockContent.qml) with a dock-specific label.
/// run_auto_check() drives deterministic, headless-friendly verification scenarios
/// for the five integration checks defined in M1 spec §2.1.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /// Dispatch to the requested auto-check. Returns 0 on success, non-zero on
    /// failure or "not implemented". Each check is added in its own M1 subtask
    /// (S3..S7); the skeleton lands check-1..5 as unimplemented stubs.
    int run_auto_check(int check_id, bool short_variant);

private slots:
    void toggle_dock1_visibility();

private:
    void build_docks();
    QDockWidget* make_dock(std::size_t index, Qt::DockWidgetArea area);
    void build_menu_bar();

    std::array<QDockWidget*, 3> docks_{};
    std::array<QQuickWidget*, 3> quick_widgets_{};
};

}  // namespace signalforge::spike
