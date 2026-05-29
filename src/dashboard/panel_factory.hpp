// src/dashboard/panel_factory.hpp
#pragma once

#include "dashboard/panel_types.hpp"
#include "decode/decoder_interface.hpp"

namespace signalforge::dashboard {

/// Suggest the default panel type for a signal of `type` (design §3):
/// Bool / String → State; Int64 / Double → Numeric. The user can
/// override per panel; this only seeds the zero-config default.
[[nodiscard]] PanelType suggestPanelType(signalforge::decoder::SignalType type);

}  // namespace signalforge::dashboard
