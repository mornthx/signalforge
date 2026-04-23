// src/frame/raw_frame.cpp
#include "frame/raw_frame.hpp"

namespace signalforge::frame {

void registerMetatypes() {
    qRegisterMetaType<RawFrame>("signalforge::frame::RawFrame");
}

}  // namespace signalforge::frame
