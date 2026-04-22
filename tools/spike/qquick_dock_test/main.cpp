#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("SignalForge"));
    app.setApplicationName(QStringLiteral("qquick_dock_test"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("M1 Qt Quick integration spike — three QQuickWidgets in QDockWidgets"));
    parser.addHelpOption();

    QCommandLineOption check_opt(QStringList{"auto-check"}, QStringLiteral("Run automated check N (1-5) and exit"),
                                 QStringLiteral("N"));
    QCommandLineOption short_opt(QStringList{"short"},
                                 QStringLiteral("Use shortened variant (currently only Check 4)"));
    parser.addOption(check_opt);
    parser.addOption(short_opt);
    parser.process(app);

    signalforge::spike::MainWindow window;
    window.show();

    if (parser.isSet(check_opt)) {
        bool ok = false;
        const int id = parser.value(check_opt).toInt(&ok);
        if (!ok) {
            qWarning() << "[spike] invalid --auto-check value:" << parser.value(check_opt);
            return 2;
        }
        const bool short_variant = parser.isSet(short_opt);
        // Defer the check dispatch by one event-loop turn so the window has a chance
        // to fully realize (important for render-thread-dependent checks).
        int rc = 0;
        QTimer::singleShot(0, &app, [&window, id, short_variant, &rc]() {
            rc = window.run_auto_check(id, short_variant);
            QCoreApplication::exit(rc);
        });
        return app.exec();
    }

    return app.exec();
}
