// tests/integration/socat_fixture.cpp
#include "tests/integration/socat_fixture.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace signalforge::test {

bool SocatVirtualPair::isAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("socat")).isEmpty();
}

SocatVirtualPair::SocatVirtualPair(int bootTimeoutMs) {
    const auto pid = ::getpid();
    side0_ = QStringLiteral("/tmp/sf_ttyV0_%1").arg(pid);
    side1_ = QStringLiteral("/tmp/sf_ttyV1_%1").arg(pid);

    QFile::remove(side0_);
    QFile::remove(side1_);

    proc_ = std::make_unique<QProcess>();
    // `-d` gives diagnostic output (silenced via null stderr if desired).
    // raw, echo=0 keeps the kernel tty layer from interfering with binary
    // payloads.
    QStringList args{QStringLiteral("-d"), QStringLiteral("-d"), QStringLiteral("pty,raw,echo=0,link=") + side0_,
                     QStringLiteral("pty,raw,echo=0,link=") + side1_};
    proc_->setProgram(QStringLiteral("socat"));
    proc_->setArguments(args);
    proc_->start();
    if (!proc_->waitForStarted(bootTimeoutMs)) {
        throw std::runtime_error("socat failed to start");
    }

    // Poll for both symlinks to appear. socat is quick (<100ms) but under
    // load can take a bit longer.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(bootTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (QFileInfo::exists(side0_) && QFileInfo::exists(side1_)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    throw std::runtime_error("socat symlinks did not appear within boot timeout");
}

SocatVirtualPair::~SocatVirtualPair() {
    if (proc_ && proc_->state() != QProcess::NotRunning) {
        proc_->kill();
        proc_->waitForFinished(1000);
    }
    QFile::remove(side0_);
    QFile::remove(side1_);
}

bool SocatVirtualPair::alive() const {
    return proc_ && proc_->state() == QProcess::Running;
}

void SocatVirtualPair::killNow() {
    if (proc_ && proc_->state() != QProcess::NotRunning) {
        proc_->kill();
        proc_->waitForFinished(1000);
    }
}

}  // namespace signalforge::test
