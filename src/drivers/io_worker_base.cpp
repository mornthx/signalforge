// src/drivers/io_worker_base.cpp
#include "drivers/io_worker_base.hpp"

#include "platform/thread_utils.hpp"

#include <utility>

namespace signalforge::drivers {

IoWorkerBase::IoWorkerBase(QString threadName, QObject* parent) : QObject(parent), threadName_(std::move(threadName)) {}

IoWorkerBase::~IoWorkerBase() = default;

const QString& IoWorkerBase::threadName() const noexcept {
    return threadName_;
}

void IoWorkerBase::onThreadStart() {
    signalforge::platform::setCurrentThreadName(threadName_);
    onStarted();
}

}  // namespace signalforge::drivers
