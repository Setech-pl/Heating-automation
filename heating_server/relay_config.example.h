#pragma once
#include "relay_output.h"
// Copy to local relay_config.h and enter confirmed GPIO and polarity.
// Rows map pump IDs 1,2,3,4 (heating),5 (domestic circulation), in that order.
// All channels are intentionally unconfigured; do not guess board pins.
const hRelayPin HEATING_RELAYS[5] = {
  {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}
};

// Existing morning CWU schedule is opt-in; the 15 minute safety limit applies.
const bool HEATING_ENABLE_DOMESTIC_PLAN = false;
