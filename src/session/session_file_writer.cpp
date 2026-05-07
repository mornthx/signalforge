// src/session/session_file_writer.cpp
//
// S1 scaffolding only. Implementation lands in S4 (encoder) + S5
// (queue + backpressure). This file exists so the worker class
// links and MOC processes its signals.
#include "session/session_file_writer.hpp"

namespace signalforge::session {

SessionFileWriter::SessionFileWriter(QObject* parent) : QObject(parent) {}

SessionFileWriter::~SessionFileWriter() = default;

bool SessionFileWriter::openFile(const QString& /*filePath*/, const SessionMetadata& /*metadata*/) {
    // S4 implements header + catalog write.
    return false;
}

bool SessionFileWriter::enqueue(SessionEvent /*event*/) {
    // S5 implements the C3 backpressure policy.
    return false;
}

void SessionFileWriter::processQueue() {
    // S4 implements the dispatch loop.
}

std::size_t SessionFileWriter::bytesWritten() const noexcept {
    return bytesWritten_.load(std::memory_order_relaxed);
}

std::size_t SessionFileWriter::droppedEvents() const noexcept {
    return droppedEvents_.load(std::memory_order_relaxed);
}

}  // namespace signalforge::session
