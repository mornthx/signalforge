// tests/integration/socat_fixture.hpp
#pragma once

#include <QProcess>
#include <QString>
#include <memory>
#include <stdexcept>
#include <string>

namespace signalforge::test {

/// RAII helper that spawns a socat virtual serial-pair subprocess
/// creating two pty pairs linked by symlinks (e.g. /tmp/sf_ttyV0_<pid>,
/// /tmp/sf_ttyV1_<pid>). Destructor kills the subprocess and removes
/// the symlinks.
///
/// The constructor blocks (up to `bootTimeoutMs`) until both symlinks
/// are visible, so tests that use the pair can rely on them being ready
/// on return.
///
/// Intentionally throws `std::runtime_error` from the constructor if
/// socat cannot be spawned or the symlinks do not appear in time, so
/// the failure manifests as a Catch2 unexpected exception in the
/// test that uses the fixture.
class SocatVirtualPair {
public:
    explicit SocatVirtualPair(int bootTimeoutMs = 2000);
    ~SocatVirtualPair();

    SocatVirtualPair(const SocatVirtualPair&) = delete;
    SocatVirtualPair& operator=(const SocatVirtualPair&) = delete;

    /// Symlink paths for the two ends of the virtual pair.
    [[nodiscard]] const QString& side0() const noexcept {
        return side0_;
    }
    [[nodiscard]] const QString& side1() const noexcept {
        return side1_;
    }

    /// Whether the socat subprocess is still running.
    [[nodiscard]] bool alive() const;

    /// Kill the subprocess early (e.g. to simulate mid-run disconnect).
    void killNow();

private:
    QString side0_;
    QString side1_;
    std::unique_ptr<QProcess> proc_;
};

}  // namespace signalforge::test
