#include "platform.h"
#include "utils.h"
#include "screen.h"
#include <climits>
#include <cstdio>
#include <new>

FakePlatform platform;
FakeWiFi WiFi;
FakeSerial Serial;
static int assertions = 0;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
  std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); \
} } while (0)

static void topics()
{
  char topic[THERMOSTAT_TOPIC_CAPACITY];
  for (int id = 1; id <= _MAX_HEATING_PUMPS_NO; ++id) {
    std::string expected = "heating/sensors/thermostat" + std::to_string(id);
    CHECK(formatThermostatTopic(topic, sizeof(topic), id));
    CHECK(std::string(topic) == expected);
    const size_t exact = expected.size() + 1;
    CHECK(formatThermostatTopic(topic, exact, id));
    CHECK(std::string(topic) == expected);
    for (size_t capacity = 1; capacity < exact; ++capacity) {
      std::memset(topic, 0x5a, sizeof(topic));
      CHECK(!formatThermostatTopic(topic, capacity, id));
      CHECK(topic[0] == '\0');
      CHECK(topic[capacity] == 0x5a); // First byte beyond advertised buffer.
    }
  }
  for (int id : {INT_MIN, -1, 0, _MAX_HEATING_PUMPS_NO + 1, INT_MAX}) {
    std::strcpy(topic, "previous topic");
    CHECK(!formatThermostatTopic(topic, sizeof(topic), id));
    CHECK(topic[0] == '\0');
  }
  topic[0] = 'Z';
  CHECK(!formatThermostatTopic(topic, 0, 1));
  CHECK(topic[0] == 'Z');
  CHECK(!formatThermostatTopic(nullptr, sizeof(topic), 1));
}

static void lcd()
{
  hConfigurator config;
  for (int pattern : {0, 0x55, 0xff}) {
    LiquidCrystal_I2C panel;
    alignas(hScreen) unsigned char storage[sizeof(hScreen)];
    std::memset(storage, pattern, sizeof(storage));
    hScreen *screen = new (storage) hScreen(&panel, &config);
    for (int row = 0; row < 4; ++row) {
      CHECK(std::string(screen->getLine(row)) == std::string(20, ' '));
      CHECK(screen->getLine(row)[20] == '\0');
    }
    screen->renderScreen();
    CHECK(panel.clears == 1);
    CHECK(panel.writes == 4);
    screen->renderScreen();
    CHECK(panel.writes == 4);
    CHECK(panel.clears == 1);
    for (size_t length : {size_t(0), size_t(19), size_t(20), size_t(21), size_t(100)}) {
      std::string text(length, 'x');
      screen->printStatusBar(text.c_str());
      std::string expected = text.substr(0, 20);
      expected.resize(20, ' ');
      CHECK(std::string(screen->getLine(3)) == expected);
      screen->renderScreen();
      CHECK(panel.rows[3] == expected);
      int writes = panel.writes;
      screen->renderScreen();
      CHECK(panel.writes == writes);
    }
    screen->clearScreen();
    int writes = panel.writes;
    screen->renderScreen();
    CHECK(panel.writes == writes + 4);
    CHECK(panel.clears == 2);
    screen->~hScreen();
  }
}

static void addresses()
{
  hConfigurator config;
  LiquidCrystal_I2C panel;
  hScreen screen(&panel, &config);
  struct Case { IPAddress address; const char *expected; };
  const Case cases[] = {
    {IPAddress(0,0,0,0), " IP : 0.0.0.0"},
    {IPAddress(192,168,100,100), " IP : 192.168.100.100"},
    {IPAddress(255,255,255,255), " IP : 255.255.255.255"},
  };
  for (bool ap : {false, true}) {
    for (const Case &test : cases) {
      WiFi = FakeWiFi();
      WiFi.station = ap ? IPAddress(1,2,3,4) : test.address;
      WiFi.accessPoint = ap ? test.address : IPAddress(1,2,3,4);
      std::string expected = std::string(test.expected).substr(0, 20);
      expected.resize(20, ' ');
      screen.printNetworkStatus(ap);
      screen.renderScreen();
      CHECK(panel.rows[3] == expected);
      CHECK(WiFi.localReads == (ap ? 0 : 1));
      CHECK(WiFi.apReads == (ap ? 1 : 0));
    }
  }
  // The menu shares the same bounded IP formatter used by startup.
  screen.nextMenu();
  screen.nextMenu();
  screen.printMenu();
  CHECK(std::string(screen.getLine(3)) == " IP : 1.2.3.4       ");
}

static void ntp()
{
  hConfigurator config;
  for (int successAttempt : {1, 3, 5, 0, 6}) {
    platform = FakePlatform();
    platform.succeedOnRequest = successAttempt;
    LiquidCrystal_I2C panel;
    hScreen screen(&panel, &config);
    tm schedule = {};
    ntp_update command(true, schedule, hourly, 0);
    for (char value : command.result) CHECK(value == '\0');
    ntp_update::StartupResult result = command.synchronizeOnStartup(false, screen);
    bool success = successAttempt >= 1 && successAttempt <= 5;
    int attempts = success ? successAttempt : 5;
    CHECK(result == (success ? ntp_update::synchronized : ntp_update::failed));
    CHECK(platform.ntpRequests == attempts);
    CHECK(platform.ntpReads == (success ? 1 : 0));
    CHECK(platform.setTimeCalls == (success ? 1 : 0));
    CHECK(platform.clockValid == success);
    CHECK(platform.clock == static_cast<time_t>(success ? platform.epoch + 3600 : 0));
    CHECK(std::string(command.result) ==
          (success ? "NTP update OK     " : "NTP update ERROR  "));
    CHECK(panel.statusWrites.size() == static_cast<size_t>(attempts + 1));
    CHECK(panel.statusWrites.back() == (success ? "NTP update OK       " : "NTP update ERROR    "));
    CHECK(std::count(platform.delays.begin(), platform.delays.end(), 300UL) == attempts);
  }
  // Skip from both an unset clock and a previously synchronized clock.
  for (bool validClock : {false, true}) {
    platform = FakePlatform();
    platform.clockValid = validClock;
    platform.clock = validClock ? 123456 : 0;
    LiquidCrystal_I2C panel;
    hScreen screen(&panel, &config);
    tm schedule = {};
    ntp_update command(true, schedule, hourly, 0);
    std::strcpy(command.result, "previous result");
    CHECK(command.synchronizeOnStartup(true, screen) == ntp_update::skipped);
    CHECK(platform.ntpRequests == 0);
    CHECK(platform.ntpReads == 0);
    CHECK(platform.setTimeCalls == 0);
    CHECK(platform.delays.empty());
    CHECK(platform.clockValid == validClock);
    CHECK(platform.clock == (validClock ? 123456 : 0));
    CHECK(std::string(command.result) == "NTP skipped (AP)");
    CHECK(panel.rows[3] == "NTP skipped (AP)    ");
  }
}

int main()
{
  topics();
  lcd();
  addresses();
  ntp();
  std::printf("PASS: %d assertions; MQTT boundaries, NTP success/limit/AP, LCD first/repeated render\n", assertions);
}
