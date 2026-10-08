#include "platform.h"
#include "scheduler.h"
#include <climits>
#include <cstdio>
#include <new>
#include <type_traits>

FakePlatform platform;
FakeWiFi WiFi;
FakeSerial Serial;
int fakeHour = 12, fakeMinute = 34;
static int assertions = 0, constructed = 0, destroyed = 0, executions = 0;
static bool failNextAllocation = false;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); \
} } while (0)

// Deterministically fail the controller's nothrow allocation; successful
// allocations still use the ordinary C++ allocator (including ASan hooks).
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
  if (failNextAllocation) { failNextAllocation = false; return nullptr; }
  return ::operator new(size);
}
void operator delete(void *pointer, const std::nothrow_t &) noexcept {
  ::operator delete(pointer);
}

static tm dueTime() {
  tm time = {};
  time.tm_hour = fakeHour;
  time.tm_min = fakeMinute;
  time.tm_mday = 8;
  time.tm_wday = 5;
  return time;
}

class Probe : public hCommand {
  bool success;
  char *resource;
public:
  Probe(bool disposable = false, int payload = 1, bool success = true,
        tm time = dueTime(), escheduleType type = hourly, hConfigurator *config = nullptr)
      : hCommand(disposable, time, type, payload, config), success(success), resource(new char[8]) {
    ++constructed;
  }
  ~Probe() override { delete[] resource; ++destroyed; }
  const void *commandType() const override { return typeKey<Probe>(); }
  bool execute() override { ++executions; return success; }
  void checkInitialState() {
    CHECK(_callbackFunction == nullptr);
    CHECK(pumpsController == nullptr);
    CHECK(_config == nullptr);
    for (char value : result) CHECK(value == 0);
  }
};

static void constructionAndBounds() {
  static_assert(std::has_virtual_destructor<hCommand>::value, "base deletion must be safe");
  static_assert(!std::is_copy_constructible<hScheduler>::value, "scheduler owns its tasks");
  for (int pattern : {0, 0x55, 0xff}) {
    alignas(hScheduler) unsigned char storage[sizeof(hScheduler)];
    std::memset(storage, pattern, sizeof(storage));
    hScheduler *scheduler = new (storage) hScheduler;
    CHECK(scheduler->maxTaskCount() == 512);
    CHECK(scheduler->activeTaskCount() == 0);
    CHECK(!scheduler->executeTasks());
    for (int slot = 0; slot < 512; ++slot) CHECK(scheduler->getTask(slot) == nullptr);
    for (int index : {INT_MIN, -1, 512, 513, INT_MAX}) {
      CHECK(scheduler->getTask(index) == nullptr);
      CHECK(!scheduler->executeTasks(index));
      scheduler->removeCommand(index);
    }
    CHECK(scheduler->addTask(nullptr) == hScheduler::invalidTask);
    CHECK(!scheduler->addExecuteTask(nullptr));
    CHECK(scheduler->activeTaskCount() == 0);
    scheduler->~hScheduler();
  }
  Probe command;
  command.checkInitialState();
}

static void capacity() {
  hScheduler scheduler;
  int before = destroyed;
  for (int slot = 0; slot < 512; ++slot) {
    Probe *task = new Probe(false, slot);
    CHECK(scheduler.addTask(task) == slot);
    CHECK(scheduler.getTask(slot) == task);
    CHECK(scheduler.activeTaskCount() == slot + 1);
  }
  CHECK(scheduler.addTask(new Probe(false, 999)) == hScheduler::full);
  CHECK(destroyed == before + 1);
  CHECK(scheduler.activeTaskCount() == 512);
  int runs = executions;
  CHECK(!scheduler.addExecuteTask(new Probe(true, 1000)));
  CHECK(executions == runs);
  CHECK(destroyed == before + 2);
  // Last valid slot must work; explicit suffix scanning preserves the API.
  CHECK(scheduler.executeTasks(511));
  CHECK(executions == runs + 1);
  scheduler.removeCommand(511);
  CHECK(scheduler.activeTaskCount() == 511);
  CHECK(scheduler.addTask(new Probe(true, 1001)) == 511);
  CHECK(scheduler.executeTasks(511));
  CHECK(scheduler.getTask(511) == nullptr);
  CHECK(scheduler.activeTaskCount() == 511);
  scheduler.removeCommand(0);
  CHECK(scheduler.addTask(new Probe(false, 1002)) == 0);
  scheduler.removeAllCommands();
  scheduler.removeAllCommands();
  CHECK(scheduler.activeTaskCount() == 0);
}

static int callbackA = 0, callbackB = 0;
static void hookA() { ++callbackA; }
static void hookB() { ++callbackB; }

static void duplicates() {
  hScheduler scheduler;
  Probe *original = new Probe;
  CHECK(scheduler.addTask(original) == 0);
  int before = destroyed;
  CHECK(scheduler.addTask(new Probe) == hScheduler::duplicate);
  CHECK(destroyed == before + 1);
  CHECK(scheduler.getTask(0) == original);
  CHECK(scheduler.addTask(original) == hScheduler::duplicate);
  CHECK(destroyed == before + 1);
  CHECK(!scheduler.addExecuteTask(new Probe));
  CHECK(destroyed == before + 2);
  CHECK(scheduler.addTask(new Probe(true)) == 1);
  CHECK(scheduler.addTask(new Probe(false, 1, true, dueTime(), daily)) == 2);
  CHECK(scheduler.addTask(new Probe(false, 1, true, dueTime(), minutly)) == 3);
  tm other = dueTime(); ++other.tm_sec;
  CHECK(scheduler.addTask(new Probe(false, 1, true, other)) == 4);
  ++other.tm_mon;
  CHECK(scheduler.addTask(new Probe(false, 1, true, other)) == 5);
  hConfigurator config;
  CHECK(scheduler.addTask(new Probe(false, 1, true, dueTime(), hourly, &config)) == 6);
  CHECK(scheduler.addTask(new hPumpCommand(false, dueTime(), hourly, 1)) == 7);
  CHECK(scheduler.addTask(new hDomesticWaterPumpCommand(false, dueTime(), hourly, 1, &config)) == 8);
  scheduler.removeAllCommands();
  for (int pattern : {0, 0x55, 0xff}) {
    alignas(hCallbackCommand) unsigned char storage[sizeof(hCallbackCommand)];
    std::memset(storage, pattern, sizeof(storage));
    hCallbackCommand *command = new (storage) hCallbackCommand(false, dueTime(), hourly, hookA);
    CHECK(command->payload == 0);
    for (char value : command->result) CHECK(value == 0);
    hCallbackCommand same(false, dueTime(), hourly, hookA);
    hCallbackCommand different(false, dueTime(), hourly, hookB);
    CHECK(command->isDuplicateOf(same));
    CHECK(!command->isDuplicateOf(different));
    command->~hCallbackCommand();
  }
  CHECK(scheduler.addTask(new hCallbackCommand(false, dueTime(), hourly, hookA)) == 0);
  CHECK(scheduler.addTask(new hCallbackCommand(false, dueTime(), hourly, hookA)) == hScheduler::duplicate);
  CHECK(scheduler.addTask(new hCallbackCommand(false, dueTime(), hourly, hookB)) == 1);
  CHECK(scheduler.addTask(new hCallbackCommand(true, dueTime(), hourly, hookA)) == 2);
  CHECK(scheduler.addTask(new hCallbackCommand(false, dueTime(), daily, hookA)) == 3);
  CHECK(scheduler.executeTasks());
  CHECK(callbackA == 3 && callbackB == 1);
  CHECK(scheduler.activeTaskCount() == 3);
}

static void executionAndRemoval() {
  hScheduler scheduler;
  int before = destroyed, runs = executions;
  CHECK(scheduler.addTask(new Probe(true, 10)) == 0);
  CHECK(scheduler.addExecuteTask(new Probe(true, 11, false)) == false);
  CHECK(executions == runs + 1); // Only the newly added failed task ran.
  CHECK(destroyed == before + 1);
  CHECK(scheduler.activeTaskCount() == 1);
  CHECK(scheduler.executeTasks());
  CHECK(destroyed == before + 2);
  CHECK(scheduler.activeTaskCount() == 0);
  CHECK(!scheduler.executeTasks());
  CHECK(scheduler.addExecuteTask(new Probe(true, 12)));
  CHECK(scheduler.activeTaskCount() == 0);
  CHECK(scheduler.addTask(new Probe(false, 13, false)) == 0);
  CHECK(scheduler.addTask(new Probe(true, 14)) == 1);
  CHECK(!scheduler.executeTasks()); // Successful sibling cannot hide failure.
  CHECK(scheduler.activeTaskCount() == 1);
  scheduler.removeCommands(999);
  CHECK(scheduler.activeTaskCount() == 1);
  scheduler.removeCommands(13);
  CHECK(scheduler.activeTaskCount() == 0);
  tm future = dueTime(); ++future.tm_hour;
  CHECK(!scheduler.addExecuteTask(new Probe(true, 15, true, future)));
  CHECK(scheduler.activeTaskCount() == 1); // Accepted but not due; still owned.
  ++fakeHour;
  CHECK(scheduler.executeTasks());
  --fakeHour;
  CHECK(scheduler.activeTaskCount() == 0);
}

// Callback removal must not destroy the currently executing derived object.
class Removing : public Probe {
  hScheduler &scheduler;
  bool all, replace;
public:
  Removing(hScheduler &scheduler, bool all, bool replace)
      : Probe(true, 80), scheduler(scheduler), all(all), replace(replace) {}
  const void *commandType() const override { return typeKey<Removing>(); }
  bool execute() override {
    int before = destroyed;
    if (all) scheduler.removeAllCommands(); else scheduler.removeCommand(0);
    CHECK(destroyed == before + (all ? 1 : 0)); // Only a sibling may be destroyed.
    CHECK(scheduler.getTask(0) == nullptr);
    CHECK(scheduler.addTask(this) == hScheduler::duplicate);
    CHECK(!scheduler.executeTasks()); // Reentrant execution is rejected.
    CHECK(payload == 80); // The object remains alive through this call.
    if (replace) CHECK(scheduler.addTask(new Probe(false, 81)) == 0);
    return true;
  }
};

static void callbackRemoval() {
  for (bool all : {false, true}) {
    hScheduler scheduler;
    CHECK(scheduler.addTask(new Removing(scheduler, all, true)) == 0);
    tm future = dueTime(); ++future.tm_hour;
    CHECK(scheduler.addTask(new Probe(false, 82, true, future)) == 1);
    CHECK(scheduler.executeTasks());
    CHECK(scheduler.getTask(0)->payload == 81); // New slot occupant survives.
    CHECK(scheduler.activeTaskCount() == (all ? 1 : 2));
  }
  hScheduler scheduler;
  CHECK(scheduler.addExecuteTask(new Removing(scheduler, false, false)));
  CHECK(scheduler.activeTaskCount() == 0);
}

static void pumpController() {
  hScheduler scheduler;
  hArduinoGpio gpio;
  const hRelayPin pins[5] = {{0,1}, {2,0}, {12,1}, {13,0}, {14,1}}; // Host fixture, no hardware mapping.
  hRelayOutputs outputs(gpio, pins);
  CHECK(outputs.begin());
  hConfigurator config(&outputs);
  thermoClientStat thermostat; thermostat.ID = 1; thermostat.serialChip = 1001;
  CHECK(config.registerClient(thermostat)); CHECK(config.recordContact(1,1001,true));
  hPumpsController controller(&scheduler, &config);
  failNextAllocation = true;
  CHECK(!controller.turnOnHeatPumpReq(1, 18, 21));
  CHECK(!failNextAllocation);
  CHECK(!config.getPumpStatus(1));
  CHECK(scheduler.activeTaskCount() == 0);
  CHECK(scheduler.addTask(new hPumpCommand(true, dueTime(), hourly, 1, &config)) == 0);
  CHECK(!controller.turnOnHeatPumpReq(1, 18, 21));
  CHECK(!config.getPumpStatus(1));
  CHECK(scheduler.activeTaskCount() == 1);
  scheduler.removeAllCommands();
  for (int i = 0; i < 512; ++i) CHECK(scheduler.addTask(new Probe(false, 1000 + i)) == i);
  int runs = executions;
  CHECK(!controller.turnOnHeatPumpReq(1, 18, 21));
  CHECK(!config.getPumpStatus(1));
  CHECK(executions == runs);
  scheduler.removeCommand(511);
  CHECK(controller.turnOnHeatPumpReq(1, 18, 21));
  CHECK(config.getPumpStatus(1));
  CHECK(scheduler.activeTaskCount() == 511);
  CHECK(executions == runs);
  platform.milliseconds += 120000;
  fakeMinute += 2; // Existing minimum ON/OFF interval, without wall-clock time.
  CHECK(scheduler.addTask(new Probe(false, 9999)) == 511);
  CHECK(!controller.turnOffHeatPumpReq(1, 18, 21));
  CHECK(config.getPumpStatus(1));
  scheduler.removeCommand(511);
  failNextAllocation = true;
  CHECK(!controller.turnOffHeatPumpReq(1, 18, 21));
  CHECK(!failNextAllocation);
  CHECK(config.getPumpStatus(1));
  CHECK(controller.turnOffHeatPumpReq(1, 18, 21));
  CHECK(!config.getPumpStatus(1));
  CHECK(executions == runs);
  fakeMinute -= 2;
  config.manualCirculationEnabled = true;
  CHECK(scheduler.addTask(new Probe(false, 9999)) == 511);
  controller.turnOffDomesticWaterPumpReq(dueTime());
  CHECK(config.manualCirculationEnabled);
}

int main() {
  for (auto test : {constructionAndBounds, capacity, duplicates,
                    executionAndRemoval, callbackRemoval, pumpController}) {
    test();
    CHECK(constructed == destroyed);
  }
  std::printf("PASS: %d assertions; %d derived constructions/destructions; scheduler and pump controller\n",
              assertions, constructed);
}
