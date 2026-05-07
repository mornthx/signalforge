// src/session/session_writer.cpp
//
// S1 scaffolding only. The implementation lands in S3-S5; this
// file exists to make `signalforge_session` a complete static
// library with a real public surface, and to keep MOC happy.
#include "session/session_writer.hpp"

#include "session/session_file_writer.hpp"

#include <QThread>

namespace signalforge::session {

SessionWriter::SessionWriter(signalforge::buffer::SignalBufferRegistry& registry, QObject* parent)
    : QObject(parent), registry_(&registry) {
    (void)registry_;  // Unused until S3 wires up the sink registration.
}

SessionWriter::~SessionWriter() = default;

bool SessionWriter::start(const QString& /*filePath*/, const QString& /*description*/,
                          const QString& /*decoderSchemaId*/) {
    // S3 implements the lifecycle.
    return false;
}

std::size_t SessionWriter::stop() {
    // S3 implements the drain + join.
    return 0;
}

bool SessionWriter::isRecording() const noexcept {
    return state_ == RecordingState::Recording;
}

RecordingState SessionWriter::state() const noexcept {
    return state_;
}

QString SessionWriter::currentFilePath() const {
    return currentFilePath_;
}

SessionMetadata SessionWriter::metadata() const {
    return metadata_;
}

std::size_t SessionWriter::eventsRecorded() const noexcept {
    return eventsRecorded_.load(std::memory_order_relaxed);
}

std::size_t SessionWriter::bytesWritten() const noexcept {
    return bytesWritten_.load(std::memory_order_relaxed);
}

std::size_t SessionWriter::droppedEvents() const noexcept {
    return droppedEvents_.load(std::memory_order_relaxed);
}

void SessionWriter::onSignal(std::chrono::steady_clock::time_point /*timestamp*/, const QString& /*signalId*/,
                             const signalforge::decoder::SignalValue& /*value*/) {
    // S4 forwards to the worker queue.
}

void SessionWriter::onSignalsRegistered(const QString& /*driverId*/,
                                        const std::vector<signalforge::decoder::SignalMetadata>& /*signalsList*/) {
    // S4 enqueues a CatalogExtensionEvent.
}

void SessionWriter::onSignalsUnregistered(const QString& /*driverId*/) {
    // No-op: V1 keeps the catalog growable; unregistration does not
    // remove signals from the recorded catalog (per spec §3.4).
}

}  // namespace signalforge::session
