#pragma once
#include <stdint.h>

struct hRelayPin {
  int gpio; // ESP8266 GPIO number, never a pump ID or Dx board label.
  int activeLevel; // 0 or 1; -1 means unconfigured.
};

class hGpio {
public:
  virtual ~hGpio() = default;
  virtual bool initializeOff(int gpio, int level) = 0;
  virtual bool write(int gpio, int level) = 0;
};

class hRelayOutputs {
public:
  enum State { unknown, off, on };
  hRelayOutputs(hGpio &gpio, const hRelayPin (&pins)[5]);
  bool begin(); // Configured channels start OFF; invalid channels remain unknown.
  bool set(int pumpId, bool running);
  State state(int pumpId) const;
private:
  hGpio &_gpio;
  hRelayPin _pins[5];
  State _states[5] = {};
  bool valid(int index) const;
};

// Production Arduino adapter. No GPIO access occurs until begin()/set().
class hArduinoGpio : public hGpio {
public:
  bool initializeOff(int gpio, int level) override;
  bool write(int gpio, int level) override;
};
