// src/dashboard/panel_factory.cpp

#include "dashboard/panel_factory.hpp"

namespace signalforge::dashboard {

PanelType suggestPanelType(signalforge::decoder::SignalType type) {
    using signalforge::decoder::SignalType;
    switch (type) {
    case SignalType::Bool:
    case SignalType::String:
        return PanelType::State;
    case SignalType::Int64:
    case SignalType::Double:
        // M21 decision D1: a scalar's zero-config default is a value
        // card (the "current value" view DR-001 found missing), not a
        // trend plot. Switch the panel to Plot for a trend.
        return PanelType::Numeric;
    }
    return PanelType::Numeric;
}

}  // namespace signalforge::dashboard
