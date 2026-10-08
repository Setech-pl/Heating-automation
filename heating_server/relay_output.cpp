#include "relay_output.h"
#include <Arduino.h>

hRelayOutputs::hRelayOutputs(hGpio &gpio, const hRelayPin (&pins)[5]) : _gpio(gpio) {
  for (int i = 0; i < 5; ++i) _pins[i] = pins[i];
}

bool hRelayOutputs::valid(int index) const {
  const hRelayPin &pin = _pins[index];
  // GPIO6-11 are connected to ESP8266 flash. Do not accept a pump ID as GPIO.
  if (pin.gpio < 0 || pin.gpio > 16 || (pin.gpio >= 6 && pin.gpio <= 11) ||
      (pin.activeLevel != 0 && pin.activeLevel != 1)) return false;
  if (pin.gpio == 1 || pin.gpio == 3) return false; // Serial TX/RX remain in use.
  if (pin.gpio == SDA || pin.gpio == SCL) return false; // LCD uses Wire default pins.
  for (int i = 0; i < 5; ++i)
    if (i != index && _pins[i].gpio == pin.gpio) return false;
  return true;
}

bool hRelayOutputs::begin() {
  bool complete = true;
  for (int i = 0; i < 5; ++i) {
    _states[i] = unknown;
    if (valid(i) && _gpio.initializeOff(_pins[i].gpio, 1 - _pins[i].activeLevel))
      _states[i] = off;
    else complete = false;
  }
  return complete;
}

bool hRelayOutputs::set(int pumpId, bool running) {
  if (pumpId < 1 || pumpId > 5 || _states[pumpId - 1] == unknown) return false;
  int i = pumpId - 1;
  if (!_gpio.write(_pins[i].gpio, running ? _pins[i].activeLevel : 1 - _pins[i].activeLevel))
    return false;
  _states[i] = running ? on : off;
  return true;
}

hRelayOutputs::State hRelayOutputs::state(int pumpId) const {
  return pumpId >= 1 && pumpId <= 5 ? _states[pumpId - 1] : unknown;
}

bool hArduinoGpio::initializeOff(int gpio, int level) {
  digitalWrite(gpio, level); // Set output latch before enabling the driver.
  pinMode(gpio, OUTPUT);
  return true;
}

bool hArduinoGpio::write(int gpio, int level) {
  digitalWrite(gpio, level);
  return true; // Output command issued; there is no physical relay feedback.
}
