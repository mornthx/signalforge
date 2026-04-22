#include "main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QTimer>
#include <atomic>

namespace {

std::atomic<int> s_concerning_warnings{0};
QFile* s_log_file = nullptr;
QtMessageHandler s_previous_handler = nullptr;
QMutex s_log_mutex;

constexpr std::array<const char*, 4> kConcernPatterns = {"QSG", "QQuickWidget", "QtQuick", "QML"};

void spike_message_handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (s_previous_handler != nullptr) {
        s_previous_handler(type, ctx, msg);
    }
    {
        QMutexLocker lock(&s_log_mutex);
        if (s_log_file != nullptr && s_log_file->isOpen()) {
            QTextStream ts(s_log_file);
            const char* level = "debug";
            switch (type) {
            case QtDebugMsg:
                level = "debug";
                break;
            case QtInfoMsg:
                level = "info";
                break;
            case QtWarningMsg:
                level = "warn";
                break;
            case QtCriticalMsg:
                level = "crit";
                break;
            case QtFatalMsg:
                level = "fatal";
                break;
            }
            ts << "[" << level << "] " << msg << "\n";
            ts.flush();
        }
    }
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        for (const char* pat : kConcernPatterns) {
            if (msg.contains(QLatin1String(pat))) {
                s_concerning_warnings.fetch_add(1);
                break;
            }
        }
    }
}

bool install_check_logging(int check_id) {
    const QString dir = QStringLiteral("docs/spikes/M1-artifacts");
    if (!QDir().mkpath(dir)) {
        return false;
    }
    const QString path = QStringLiteral("%1/check%2-log.txt").arg(dir).arg(check_id);
    auto* f = new QFile(path);
    if (!f->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        delete f;
        return false;
    }
    {
        QMutexLocker lock(&s_log_mutex);
        s_log_file = f;
    }
    s_previous_handler = qInstallMessageHandler(spike_message_handler);
    return true;
}

void close_check_logging() {
    qInstallMessageHandler(s_previous_handler);
    QMutexLocker lock(&s_log_mutex);
    if (s_log_file != nullptr) {
        s_log_file->close();
        delete s_log_file;
        s_log_file = nullptr;
    }
}

}  // namespace

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

        if (!install_check_logging(id)) {
            qWarning() << "[spike] could not open log file under docs/spikes/M1-artifacts/";
            return 2;
        }

        // Defer the check dispatch by ~500 ms so the first QML paint completes
        // before any render-thread-dependent assertions run.
        int check_rc = 0;
        QTimer::singleShot(500, &app, [&window, id, short_variant, &check_rc]() {
            check_rc = window.run_auto_check(id, short_variant);
            QCoreApplication::exit(0);
        });
        app.exec();

        const int warnings = s_concerning_warnings.load();
        qDebug().nospace() << "[spike] check " << id << " rc=" << check_rc << " concerning_warnings=" << warnings;
        close_check_logging();

        if (check_rc != 0) {
            return check_rc;
        }
        return warnings > 0 ? 1 : 0;
    }

    return app.exec();
}
