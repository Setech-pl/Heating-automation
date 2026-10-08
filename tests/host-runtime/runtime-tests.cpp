#include "platform.h"
#include <cstdio>
#include <cmath>
#include <climits>

FakePlatform platform;
FakeWiFi WiFi;
FakeSerial Serial;
FakeESP ESP;
void hook_mqtt_reconnect(); // Arduino normally generates this prototype.
#include "heating_server.ino" // Exact production setup/loop, not a test copy.

static int checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); \
} } while (0)

static time_t epoch(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) {
  tmElements_t parts = {static_cast<uint8_t>(second), static_cast<uint8_t>(minute),
    static_cast<uint8_t>(hour), 0, static_cast<uint8_t>(day), static_cast<uint8_t>(month),
    static_cast<uint8_t>(year - 1970)};
  return makeTime(parts);
}
static void clockAt(time_t value) { platform.clock = value; platform.clockValid = true; platform.calendarValid = true; }
static tm scheduleAt(int hour, int minute, int weekday = 1, int day = 1) {
  tm schedule = {};
  schedule.tm_hour = hour; schedule.tm_min = minute;
  schedule.tm_wday = weekday; schedule.tm_mday = day;
  return schedule;
}

class Counting : public hCommand {
  int &runs;
public:
  Counting(int &runs, tm time, escheduleType recurrence) : hCommand(false, time, recurrence, 0), runs(runs) {}
  bool execute() override { ++runs; return true; }
};

class TestGpio : public hGpio {
public:
  int writes = 0, inits = 0, lastPin = -1, lastLevel = -1;
  bool failInit = false, failWrite = false;
  bool initializeOff(int pin, int level) override {
    ++inits; lastPin = pin; lastLevel = level; return !failInit;
  }
  bool write(int pin, int level) override {
    ++writes; lastPin = pin; lastLevel = level; return !failWrite;
  }
};
// Synthetic host-only mapping, never a proposed physical board configuration.
static const hRelayPin fixturePins[5] = {{0,1}, {2,0}, {12,1}, {13,0}, {14,1}};

static void calendar() {
  int runs = 0;
  clockAt(epoch(2026,10,10,23,59)); // Saturday (TimeLib day 7).
  hScheduler dailyScheduler;
  CHECK(dailyScheduler.addTask(new Counting(runs, scheduleAt(0,0), daily)) == 0);
  CHECK(!dailyScheduler.executeTasks());
  for (int day = 0; day < 40; ++day) {
    clockAt(epoch(2026,10,11) + day * SECS_PER_DAY);
    CHECK(dailyScheduler.executeTasks());
    CHECK(runs == day + 1);
    CHECK(!dailyScheduler.executeTasks());
    CHECK(dailyScheduler.getTask(0)->scheduleTime.tm_wday >= 1);
    CHECK(dailyScheduler.getTask(0)->scheduleTime.tm_wday <= 7);
  }
  time_t beforeJump = now();
  clockAt(beforeJump + 10 * SECS_PER_DAY);
  CHECK(dailyScheduler.executeTasks()); CHECK(runs == 41); // One current occurrence, no replay.
  clockAt(beforeJump);
  CHECK(!dailyScheduler.executeTasks());
  clockAt(beforeJump + 11 * SECS_PER_DAY);
  CHECK(dailyScheduler.executeTasks()); CHECK(runs == 42);

  clockAt(epoch(2026,10,8,12,10));
  hScheduler hourlyScheduler;
  int hourlyRuns = 0;
  CHECK(hourlyScheduler.addTask(new Counting(hourlyRuns, scheduleAt(12,11), hourly)) == 0);
  CHECK(!hourlyScheduler.executeTasks());
  clockAt(epoch(2026,10,8,12,12));
  CHECK(!hourlyScheduler.executeTasks()); // Missed switching minute is skipped.
  CHECK(hourlyScheduler.getTask(0)->scheduleTime.tm_hour == 13);
  clockAt(epoch(2026,10,8,13,11));
  CHECK(hourlyScheduler.executeTasks()); CHECK(hourlyRuns == 1);
  CHECK(!hourlyScheduler.executeTasks());

  clockAt(epoch(2026,12,31,23,59));
  hScheduler minutes;
  int minuteRuns = 0;
  CHECK(minutes.addTask(new Counting(minuteRuns, scheduleAt(23,59), minutly)) == 0);
  CHECK(minutes.executeTasks());
  clockAt(epoch(2027,1,1));
  CHECK(minutes.executeTasks()); CHECK(minuteRuns == 2);
  clockAt(epoch(2027,1,1,2,30));
  CHECK(minutes.executeTasks()); CHECK(minuteRuns == 3);
  CHECK(!minutes.executeTasks());

  clockAt(epoch(2026,10,10,23,59));
  hScheduler weeks;
  int weekRuns = 0;
  CHECK(weeks.addTask(new Counting(weekRuns, scheduleAt(0,0,1), weekly)) == 0);
  for (int day = 0; day < 22; ++day) {
    clockAt(epoch(2026,10,11) + day * SECS_PER_DAY);
    CHECK(weeks.executeTasks() == (day % 7 == 0));
  }
  CHECK(weekRuns == 4);

  clockAt(epoch(2027,1,31));
  hScheduler months;
  int monthRuns = 0;
  CHECK(months.addTask(new Counting(monthRuns, scheduleAt(0,0,1,31), monthly)) == 0);
  CHECK(months.executeTasks());
  clockAt(epoch(2027,2,28)); CHECK(!months.executeTasks());
  clockAt(epoch(2027,3,31)); CHECK(months.executeTasks()); CHECK(monthRuns == 2);
  clockAt(epoch(2028,2,28));
  hScheduler leap;
  CHECK(leap.addTask(new Counting(monthRuns, scheduleAt(0,0,1,29), monthly)) == 0);
  clockAt(epoch(2028,2,29)); CHECK(leap.executeTasks());
  clockAt(epoch(2028,3,29)); CHECK(leap.executeTasks());
  CHECK(monthRuns == 4);

  hScheduler invalid;
  for (tm bad : {scheduleAt(24,0), scheduleAt(0,60), scheduleAt(-1,0)})
    CHECK(invalid.addTask(new Counting(runs, bad, daily)) == hScheduler::invalidTask);
  CHECK(invalid.addTask(new Counting(runs, scheduleAt(0,0,0), weekly)) == hScheduler::invalidTask);
  CHECK(invalid.addTask(new Counting(runs, scheduleAt(0,0,1,0), monthly)) == hScheduler::invalidTask);
  platform.calendarValid = false;
  CHECK(invalid.addTask(new Counting(runs, scheduleAt(0,0), daily)) == 0);
  CHECK(!invalid.executeTasks());
  clockAt(epoch(2029,1,1)); CHECK(invalid.executeTasks());
}

static void gpioAndCommands() {
  TestGpio adapter;
  const hRelayPin missing[5] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1}};
  hRelayOutputs absent(adapter, missing);
  CHECK(!absent.begin()); CHECK(adapter.inits == 0);
  for (int id : {INT_MIN,-1,0,1,5,6,INT_MAX}) {
    CHECK(!absent.set(id, true)); CHECK(absent.state(id) == hRelayOutputs::unknown);
  }
  hRelayOutputs outputs(adapter, fixturePins);
  CHECK(!outputs.set(1, true));
  CHECK(outputs.begin()); CHECK(adapter.inits == 5);
  for (int i = 1; i <= 5; ++i) CHECK(outputs.state(i) == hRelayOutputs::off);
  CHECK(outputs.set(2, true)); CHECK(adapter.lastPin == 2 && adapter.lastLevel == 0);
  CHECK(outputs.set(2, false)); CHECK(adapter.lastLevel == 1);
  adapter.failWrite = true;
  CHECK(!outputs.set(2,true)); CHECK(outputs.state(2) == hRelayOutputs::off);
  adapter.failWrite = false;
  for (int badPin : {-1,1,3,4,5,6,7,8,9,10,11,17}) {
    hRelayPin pins[5]; std::copy(fixturePins, fixturePins + 5, pins); pins[0].gpio = badPin;
    hRelayOutputs rejected(adapter, pins);
    CHECK(!rejected.begin()); CHECK(!rejected.set(1,true));
  }
  hRelayPin duplicated[5]; std::copy(fixturePins, fixturePins + 5, duplicated);
  duplicated[1].gpio = duplicated[0].gpio;
  hRelayOutputs duplicate(adapter, duplicated);
  CHECK(!duplicate.begin()); CHECK(!duplicate.set(1,true)); CHECK(!duplicate.set(2,true));
  duplicated[1] = fixturePins[1]; duplicated[0].activeLevel = -1;
  hRelayOutputs polarityMissing(adapter, duplicated);
  CHECK(!polarityMissing.begin()); CHECK(!polarityMissing.set(1,true));
  adapter.failInit = true;
  hRelayOutputs failedInit(adapter, fixturePins);
  CHECK(!failedInit.begin()); CHECK(!failedInit.set(1,true));
  adapter.failInit = false;

  gpioCalls().clear();
  hArduinoGpio native;
  hRelayOutputs arduino(native, fixturePins);
  CHECK(arduino.begin()); CHECK(gpioCalls().size() == 10);
  for (int i = 0; i < 5; ++i) {
    CHECK(!gpioCalls()[2*i].mode);
    CHECK(gpioCalls()[2*i].pin == fixturePins[i].gpio);
    CHECK(gpioCalls()[2*i].value == 1 - fixturePins[i].activeLevel);
    CHECK(gpioCalls()[2*i+1].mode);
  }
  CHECK(arduino.set(5,true)); CHECK(gpioCalls().back().pin == 14 && gpioCalls().back().value == 1);

  clockAt(epoch(2026,10,8,15)); platform.milliseconds = 0;
  hConfigurator state(&outputs); hScheduler tasks; hPumpsController controller(&tasks,&state);
  adapter.failWrite = true;
  CHECK(!controller.turnOnHeatPumpReq(1,18,21)); CHECK(!state.getPumpStatus(1));
  CHECK(tasks.activeTaskCount() == 0);
  adapter.failWrite = false;
  CHECK(controller.turnOnHeatPumpReq(1,18,21)); CHECK(state.getPumpStatus(1));
  CHECK(outputs.state(1) == hRelayOutputs::on);
  CHECK(!controller.turnOnHeatPumpReq(1,18,21)); // Existing repeated request contract: NO.
  CHECK(!controller.turnOffHeatPumpReq(1,18,21));
  platform.milliseconds = 60000;
  adapter.failWrite = true;
  CHECK(!controller.turnOffHeatPumpReq(1,18,21)); CHECK(state.getPumpStatus(1));
  adapter.failWrite = false;
  CHECK(controller.turnOffHeatPumpReq(1,18,21)); CHECK(!state.getPumpStatus(1));
  CHECK(!controller.turnOnHeatPumpReq(1,18,21));
  platform.milliseconds += 60000;
  CHECK(controller.turnOnHeatPumpReq(1,18,21));
  for (int id : {INT_MIN,0,5,INT_MAX}) CHECK(!controller.turnOffHeatPumpReq(id,18,21));
  CHECK(!controller.turnOnHeatPumpReq(2,NAN,21));
  CHECK(!controller.turnOffHeatPumpReq(2,18,INFINITY));
  hConfigurator unconfigured(&absent); hPumpsController rejected(&tasks,&unconfigured);
  CHECK(!rejected.turnOnHeatPumpReq(2,18,21)); CHECK(!unconfigured.getPumpStatus(2));
}

static void timeAndLimits() {
  clockAt(epoch(2026,10,8,15)); platform.milliseconds = 0xfffffff0UL;
  TestGpio adapter; hRelayOutputs outputs(adapter,fixturePins); CHECK(outputs.begin());
  hConfigurator state(&outputs); hScheduler tasks; hPumpsController controller(&tasks,&state);
  CHECK(controller.turnOnHeatPumpReq(1,18,21));
  state.tickMinutes(); CHECK(state.getPumpRunningMinuts(1) == 0);
  platform.milliseconds = uint32_t(0xfffffff0U + 59999U);
  state.tickMinutes(); CHECK(state.getPumpRunningMinuts(1) == 0);
  platform.milliseconds = uint32_t(0xfffffff0U + 60000U);
  state.tickMinutes(); CHECK(state.getPumpRunningMinuts(1) == 1);
  platform.milliseconds += 119 * 60000;
  clockAt(epoch(2025,1,1)); // Calendar jumps do not change runtime or history.
  state.tickMinutes(); CHECK(state.getPumpRunningMinuts(1) == 120);
  CHECK(state.lastOnOffPump(1,1) == 0); CHECK(state.getPercentage(1) == 100);
  CHECK(controller.turnOnHeatPumpReq(2,18,21));
  platform.milliseconds += 120 * 60000;
  state.tickMinutes(); CHECK(state.getPercentage(1) == 66 && state.getPercentage(2) == 33);
  for (int id : {INT_MIN,0,6,INT_MAX}) {
    CHECK(state.getPumpRunningMinuts(id) == 0); CHECK(state.getPercentage(id) == 0);
  }
  platform.milliseconds += 7 * 86400000;
  CHECK(state.lastOnOffPump(1,1) == 0);
  for (int i = 0; i < 300; ++i) {
    state.setPumpStatusOn(3,18,21); state.setPumpStatusOff(3);
  }
  CHECK(state.lastOnOffPump(3,1) == 128); // Ring capacity, ON events only.
  state.setPumpStatusOff(1);
  state.setPumpStatusOn(1,18,21);
  for (int i = 0; i < 300; ++i) { state.setPumpStatusOn(3,18,21); state.setPumpStatusOff(3); }
  CHECK(state.lastOnOffPump(1,1) == 0); // History wrap cannot disable the minimum ON time.
  CHECK(!state.canStopPump(1));
  platform.milliseconds += 60000;
  CHECK(state.canStopPump(1));
  CHECK(state.lastOnOffPump(3,1) == 0);

  platform.milliseconds = 0; clockAt(epoch(2026,10,8,15));
  hConfigurator limited(&outputs); hPumpsController limiter(&tasks,&limited);
  CHECK(limiter.turnOnHeatPumpReq(4,18,21));
  platform.milliseconds = uint32_t(_MAX_HEATING_PUMP_RUNNING_MINUTES) * 60000 - 1;
  limiter.sanityCheck(); CHECK(limited.getPumpStatus(4));
  platform.milliseconds += 1;
  platform.calendarValid = false;
  limiter.sanityCheck(); CHECK(!limited.getPumpStatus(4));
  int writes = adapter.writes; limiter.sanityCheck(); CHECK(adapter.writes == writes);
  platform.milliseconds += 60000;
  hDomesticWaterPumpCommand domestic(false, scheduleAt(0,0), daily, 5, &limited);
  CHECK(domestic.execute()); CHECK(limited.getPumpStatus(5));
  CHECK(!domestic.execute()); // A repeated ON cannot restart the safety timer.
  hDomesticWaterPumpCommand stopDomestic(false, scheduleAt(0,0), daily, 15, &limited);
  CHECK(!stopDomestic.execute()); // Same minimum ON interval applies to CWU.
  platform.milliseconds += 15 * 60000;
  limiter.sanityCheck(); CHECK(!limited.getPumpStatus(5));
  // Expiry does not need a free task slot and cannot be blocked by min-ON validation.
  platform.milliseconds += 60000;
  CHECK(limiter.turnOnHeatPumpReq(4,18,21));
  int unused = 0;
  for (int i = 0; i < 512; ++i) CHECK(tasks.addTask(new Counting(unused, scheduleAt(0,0), daily)) == i);
  platform.milliseconds += uint32_t(_MAX_HEATING_PUMP_RUNNING_MINUTES) * 60000;
  limiter.sanityCheck(); CHECK(!limited.getPumpStatus(4));
}

static void nightAndPlan() {
  for (int h : {21,22,23,0,5,6}) {
    platform.milliseconds = 0; clockAt(epoch(2026,10,8,h));
    TestGpio adapter; hRelayOutputs outputs(adapter,fixturePins); CHECK(outputs.begin());
    hConfigurator state(&outputs); hScheduler tasks; hPumpsController controller(&tasks,&state);
    CHECK(controller.turnOnHeatPumpReq(1,20,21) == !(h > 22 || h < 6));
  }
  clockAt(epoch(2026,10,8,4,59)); platform.milliseconds = 0;
  TestGpio adapter; hRelayOutputs outputs(adapter,fixturePins); CHECK(outputs.begin());
  hConfigurator state(&outputs); hScheduler tasks; hPumpsController controller(&tasks,&state);
  CHECK(!controller.createDailyPlan(true)); CHECK(tasks.activeTaskCount() == 0);
  CHECK(controller.createDailyPlan(false)); CHECK(tasks.activeTaskCount() == 14);
  CHECK(!controller.createDailyPlan(false)); CHECK(tasks.activeTaskCount() == 14);
  for (int d = 0; d < 2; ++d) {
    clockAt(epoch(2026,10,8+d,5)); platform.milliseconds += 86400000;
    CHECK(tasks.executeTasks()); CHECK(state.getPumpStatus(5));
    platform.milliseconds += 15 * 60000;
    controller.sanityCheck(); CHECK(!state.getPumpStatus(5));
    clockAt(epoch(2026,10,8+d,5,30));
    CHECK(tasks.executeTasks()); CHECK(!state.getPumpStatus(5));
  }
  CHECK(!controller.createDailyPlan(false)); CHECK(tasks.activeTaskCount() == 14);
}

static void ticksAndReconnect() {
  uint32_t previous = 0xfffffbffU;
  CHECK(heatingTickDue(0x20,previous,1000)); CHECK(previous == 0x20);
  CHECK(!heatingTickDue(0x21,previous,1000));
  CHECK(!heatingTickDue(0x407,previous,1000));
  CHECK(heatingTickDue(0x408,previous,1000));
  CHECK(heatingTickDue(0x10000,previous,1000)); CHECK(!heatingTickDue(0x10000,previous,1000));
  WiFiClient network; PubSubClient broker(network); hMqttReconnect retries;
  CHECK(!retries.poll(broker,0,false,"id","user","pass","topic")); CHECK(broker.attempts == 0);
  CHECK(!retries.poll(broker,0,true,"id","user","pass","topic")); CHECK(broker.attempts == 1);
  CHECK(!retries.poll(broker,59999,true,"id","user","pass","topic")); CHECK(broker.attempts == 1);
  broker.connectSuccess = true; broker.subscribeSuccess = false;
  CHECK(!retries.poll(broker,60000,true,"id","user","pass","topic")); CHECK(!broker.connected());
  broker.subscribeSuccess = true;
  CHECK(retries.poll(broker,120000,true,"id","user","pass","topic"));
  CHECK(broker.attempts == 3 && broker.subscriptions == 2);
  CHECK(broker.user == "user" && broker.password == "pass");
  CHECK(retries.poll(broker,120001,true,"id","user","pass","topic")); CHECK(broker.attempts == 3);
  hMqttReconnect wrap; broker.disconnect(); broker.connectSuccess = false;
  CHECK(!wrap.poll(broker,0xfffffff0U,true,"id","","","topic"));
  int before = broker.attempts;
  CHECK(!wrap.poll(broker,uint32_t(0xfffffff0U+59999U),true,"id","","","topic")); CHECK(broker.attempts == before);
  CHECK(!wrap.poll(broker,uint32_t(0xfffffff0U+60000U),true,"id","","","topic")); CHECK(broker.attempts == before+1);
}

static void datagram(const char *cmd, int id) {
  char packet[180];
  snprintf(packet,sizeof(packet),"{\"cmd\":\"%s\",\"ID\":%d,\"actualTEMP\":18,\"targetTEMP\":21}",cmd,id);
  datagrams().packet = packet; datagrams().reportedSize = std::strlen(packet);
  loop();
}
static void ack(const char *command, const char *running, const char *output) {
  StaticJsonBuffer<512> buffer;
  JsonObject &message = buffer.parseObject(datagrams().response.c_str());
  CHECK(message.success());
  CHECK(std::string(message["cmd"].as<const char *>()) == command);
  CHECK(std::string(message["RUNNING"].as<const char *>()) == running);
  CHECK(std::string(message["OUTPUT"].as<const char *>()) == output);
}

static void setupAndLoop() {
  platform = FakePlatform(); WiFi = FakeWiFi(); client.connectSuccess = false;
  platform.epoch = static_cast<unsigned long>(epoch(2026,10,8,14));
  gpioCalls().clear(); setup();
  CHECK(gpioCalls().empty()); // Published defaults never touch unknown pins.
  CHECK(scheduler->activeTaskCount() == 3);
  CHECK(espClient.timeout == 200); CHECK(client.attempts == 1);
  CHECK(config->outputState(1) == hRelayOutputs::unknown);
  datagram("ON",1); ack("NO","NO","UNCONFIGURED");
  datagram("SHOWSTATUS",INT_MAX); ack("NO","NO","UNCONFIGURED");
  datagram("ON",INT_MIN); ack("NO","NO","UNCONFIGURED");
  CHECK(client.attempts == 1); // No loop-driven retry storm.

  TestGpio adapter; hRelayOutputs outputs(adapter,fixturePins); CHECK(outputs.begin());
  hConfigurator state(&outputs); hPumpsController controller(scheduler,&state);
  config = &state; heatPumpController = &controller;
  datagram("ON",1); ack("OK","YES","ON");
  datagram("OFF",1); ack("NO","YES","ON");
  platform.milliseconds += 60000;
  datagram("OFF",1); ack("OK","NO","OFF");
  platform.milliseconds += 60000;
  datagram("ON",1); ack("OK","YES","ON");
  platform.milliseconds += uint32_t(_MAX_HEATING_PUMP_RUNNING_MINUTES) * 60000;
  platform.calendarValid = false;
  loop(); CHECK(!state.getPumpStatus(1)); // Exact production loop calls monotonic safety.
  config = &configInstance; heatPumpController = &controllerInstance;

  internalWIFIMode = true; WiFi.accessPoint = IPAddress(192,168,77,1);
  udpMessenger.begin(true);
  datagram("SHOWSTATUS",1);
  CHECK(datagrams().response.find("192.168.77.1") != std::string::npos);
  hook_discover_devices(); CHECK(datagrams().destination == IPAddress(192,168,77,255));
  int requests = platform.ntpRequests, attempts = client.attempts;
  hook_ntp_update(); hook_mqtt_reconnect();
  CHECK(platform.ntpRequests == requests && client.attempts == attempts);
  char topic[] = "heating/commands";
  byte payload = 0; mqttCallback(topic,&payload,513); mqttCallback(nullptr,&payload,1); mqttCallback(topic,nullptr,1);
  CHECK(!config->getPumpStatus(1));
  internalWIFIMode = false;
}

int main() {
  calendar(); gpioAndCommands(); timeAndLimits(); nightAndPlan(); ticksAndReconnect(); setupAndLoop();
  std::printf("PASS: %d assertions; calendar, GPIO, limits, retry and production setup/loop\n",checks);
}
