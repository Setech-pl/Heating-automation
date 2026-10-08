# Controller MQTT, NTP and LCD regressions

[Polski](README.pl.md) | English | [Project README](../../README.md)

From the repository root, after preparing dependencies:

```sh
python3 scripts/test-host-startup.py
python3 scripts/test-host-startup.py --sanitize
```

Requirements: Python ≥3.9, Clang (`CXX` can select another Clang) and a pinned,
clean NTPClient 3.2.0 checkout in `.arduino/user/libraries/NTPClient`.
The script does not download dependencies. Production `utils`, `screen`,
`scheduler`, `heating_config`, `relay_output` and `createDailyPlan` files are
copied into ignored `build/host-startup/{normal,sanitize}/source`, as in the
firmware build, so `secrets.h` selects only the example. Tests contain no
separate implementation of the logic and do not read local credentials.

The target executes production topic formatting, bounded startup
synchronization, the NTP command and screen rendering. It compiles the real
NTPClient: fake UDP supplies an NTP packet or timeout. Adapters replace only
Arduino, Wi-Fi/UDP, the clock and LCD interfaces. Time is deterministic;
`delay` advances a counter without waiting. There are no network connections
or hardware access.

Regressions check topics for pumps 1–4 (the existing controller domain),
exact-size and undersized buffers, truncated-topic and invalid-ID rejection;
NTP success on attempts 1/3/5, timeout and the five-attempt limit, AP mode
without attempts or clock changes; command-result initialization and first
and unchanged renders on memory with different patterns; LCD text lengths
0/19/20/21/100, boundary IPv4 values, STA/AP selection and IP formatting in the
menu. LCD truncates/pads text to 20 characters. MQTT topic formatting returns
failure and an empty buffer on truncation. NTP results distinguish skipped,
synchronized and failed.

`--sanitize` runs ASan/UBSan, stopping at the first error; LeakSanitizer is
disabled on macOS. ASan/UBSan do not detect every uninitialized read; memory
patterns and observed rendering provide a separate regression. This target
tests modules used by the sketch, not the full `setup()` or router/scheduler.
Existing warnings from other modules remain visible. Run firmware compilation
and UDP regressions separately.

The full `setup()`/`loop()`, calendar, limits, GPIO and actual MQTT timeout
are covered by `scripts/test-host-runtime.py`, also with `--sanitize`.
Runtime also checks thermostat contact through the actual parser, scheduler
and observable driver: 179 999/180 000 ms, contact renewal, HEARTBEAT without ON,
no restart after timeout, new ON with a one-minute OFF interval, cancellation
of pending and retained ON, two circuits, invalid fields and IDs, wrong serial,
startup/restart OFF, clock rollover, retry after failed OFF and single
loss/recovery logs. Existing temperature gates, CO 7720 min and CWU 15 min
limit regressions remain active.
Controller tests simulate client HEARTBEAT; they cannot test 60 s transmission
or reconnect queue discard in the absent thermostat firmware.
Hardware tests: NOT RUN. See the [architecture](../../docs/ARCHITECTURE.md).
