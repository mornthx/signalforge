// src/drivers/driver_interface.cpp
#include "drivers/driver_interface.hpp"

namespace signalforge::drivers {

DriverInterface::DriverInterface(QObject* parent) : QObject(parent) {}

DriverInterface::~DriverInterface() = default;

void registerMetatypes() {
    qRegisterMetaType<DriverState>("signalforge::drivers::DriverState");
    qRegisterMetaType<DriverError>("signalforge::drivers::DriverError");
}

}  // namespace signalforge::drivers
