// src/workbench/workbench_frame.hpp
#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

class QFrame;
class QHBoxLayout;
class QLabel;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;

namespace signalforge::workbench {

class ActivityRail;

/// The redesigned application shell (M34): a top bar, a left activity rail that
/// selects the primary mode, a stacked content area (one page per mode), a
/// collapsible right inspector, and a collapsible bottom drawer.
///
/// This is a pure layout shell — it owns no business logic. The owner adds modes
/// (each with its content widget), supplies the inspector/drawer content, and
/// drops chips/actions into the top bar. Mounting it into the real application
/// and wiring the views is done by the app layer (S4); the shell is built and
/// tested in isolation first.
class WorkbenchFrame : public QWidget {
    Q_OBJECT

public:
    explicit WorkbenchFrame(QWidget* parent = nullptr);
    ~WorkbenchFrame() override;

    // ----- Top bar -----------------------------------------------------------

    /// Set the leading title text shown at the top-left.
    void setTitle(const QString& title);

    /// Append a widget (status chip, action button, …) to the trailing,
    /// right-aligned region of the top bar, in call order.
    void addTopBarWidget(QWidget* widget);

    // ----- Modes (rail + content stack) -------------------------------------

    /// Register a mode: a rail button (`label`, optional `glyph`) plus its
    /// `content` page. The first mode added becomes current. Re-adding an
    /// existing id replaces nothing — ids must be unique.
    void addMode(const QString& id, const QString& label, QWidget* content, const QString& glyph = QString());

    /// Switch to a mode without emitting `modeChanged` (the caller already
    /// knows). No-op for an unknown id.
    void setCurrentMode(const QString& id);

    [[nodiscard]] QString currentMode() const {
        return currentMode_;
    }

    /// The activity rail, for wiring/tests.
    [[nodiscard]] ActivityRail* rail() const {
        return rail_;
    }

    // ----- Inspector (right) -------------------------------------------------

    /// Place `widget` as the inspector content (replacing any previous). Passing
    /// nullptr clears it.
    void setInspector(QWidget* widget);
    void setInspectorVisible(bool visible);
    [[nodiscard]] bool isInspectorVisible() const;

    // ----- Drawer (bottom) ---------------------------------------------------

    /// Place `widget` as the drawer content (replacing any previous). Passing
    /// nullptr clears it.
    void setDrawer(QWidget* widget);
    void setDrawerVisible(bool visible);
    [[nodiscard]] bool isDrawerVisible() const;

Q_SIGNALS:
    /// Emitted when the user selects a different mode via the rail.
    void modeChanged(const QString& id);

private:
    void setSlotContent(QFrame* slot, QHBoxLayout* slotLayout, QWidget*& held, QWidget* widget);

    ActivityRail* rail_ = nullptr;
    QStackedWidget* contentStack_ = nullptr;
    QFrame* topBar_ = nullptr;
    QHBoxLayout* topBarLayout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QFrame* inspectorFrame_ = nullptr;
    QHBoxLayout* inspectorLayout_ = nullptr;
    QWidget* inspectorContent_ = nullptr;
    QFrame* drawerFrame_ = nullptr;
    QHBoxLayout* drawerLayout_ = nullptr;
    QWidget* drawerContent_ = nullptr;
    QSplitter* contentSplitter_ = nullptr;
    QSplitter* verticalSplitter_ = nullptr;
    QHash<QString, QWidget*> pages_;
    QString currentMode_;
};

}  // namespace signalforge::workbench
