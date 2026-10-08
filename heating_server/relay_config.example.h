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

// Configured thermostat chip serial for each CO ID 1-4. Zero disables that ID.
// One serial may own only one circuit. Packets cannot change this assignment.
const uint32_t HEATING_THERMOSTAT_SERIALS[4] = {0, 0, 0, 0};
