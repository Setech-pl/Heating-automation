# Host tests for the UDP parser

[Polski](README.pl.md) | English | [Project README](../../README.md)

From the repository root, on macOS with Python 3 and Clang:

```sh
python3 scripts/test-host-udp.py --arduino-json /path/to/ArduinoJson
python3 scripts/test-host-udp.py --arduino-json /path/to/ArduinoJson --sanitize
```

The argument selects a library directory containing `src/ArduinoJson.h` and
`.git`. It can be omitted if `.arduino/user/libraries/ArduinoJson` exists.
When sources are unavailable, fetch just this library into the ignored directory:

```sh
git clone --depth 1 --branch v5.13.5 https://github.com/bblanchon/ArduinoJson.git .arduino/user/libraries/ArduinoJson
```

The target verifies version 5.13.5, commit
`ad4b13c8f044e67f1610fba96e8dc108ebc90cd5` and a clean checkout against
`build-support/b0-lock.json`. It does not download dependencies or prepare the
firmware toolchain. Compilation failure, mismatched dependencies or a failed
assertion exits nonzero. Compilation commands are printed; binaries are written
to ignored `build/host-udp/{normal,sanitize}`. `CXX` can select another Clang.

Tests compile `heating_server/udpmessengerservice.cpp` directly and use the real
ArduinoJson. Adapters replace only UDP, Wi-Fi, Serial, ESP and the response
clock. Stubs contain no copy of the parser, JSON or command construction.
The test receiver counts published commands and attempts to dispatch ON/OFF;
it does not execute the controller, scheduler or GPIO. Tests also verify that
the complete command state and reply address survive rejected datagrams.

Compilation uses C++11 without RTTI or exceptions; ArduinoJson settings retain
`float`, disabled `long long` and nesting limit 10, as on ESP8266. Historical
library headers are treated as system headers, and the macOS warning about
existing `sprintf` is disabled; other `-Wall -Wextra` warnings are errors.
Sanitized mode runs ASan and UBSan, stopping at the first error. LeakSanitizer
is disabled on macOS. Host pointer sizes and the JSON pool differ from ESP8266.

The parser accepts a single complete JSON object of 1–512 B without NUL
and with optional JSON whitespace around it. All commands require
`cmd`, `ID`, `actualTEMP` and `targetTEMP`: ON, OFF, SHOWSERVER, SHOWSTATUS
and HEARTBEAT. `cmd` is an exact name shorter than 20 B. `ID` is a complete
integer fitting `int`; temperatures are complete decimal numbers fitting
finite `float` without a conversion range error. JSON numbers and numeric
strings remain supported.

HEARTBEAT requires ID 1–4 and `serial`. A supplied `serial` in any command
must be a positive integer 1–4294967295 or a string of 1–10 digits;
its value is copied into owned command storage. Tests cover missing fields,
zero, overflow, length, types, fractions and invalid text. Older ON/OFF without
`serial` are parsed, but rejected by the router without renewing contact.
Serial-to-circuit assignment is tested by runtime, not the parser alone.
The router permits ON/OFF IDs 1–4 and SHOWSTATUS 1–5. `versionC`, humidity
and other extra fields are ignored; they must still be valid JSON and fit the
library pool. Comments, single quotes, unquoted keys, incomplete tokens and
data after the object are rejected. `\uXXXX` escapes are rejected because
pinned ArduinoJson 5.13.5 does not decode them. Depth is bounded by the library.
The syntax validator does not construct command data or replace library decoding.

Rejection preserves the last valid command, its flag and reply address.
It creates no new publication or control effect. The command owns its memory
and retains no pointers into the datagram or local JSON pool.

This target does not establish firmware compilation, flash/RAM budgets,
full-router operation, pump policy or physical outputs. Thermostat-loss
behavior is described in the [architecture](../../docs/ARCHITECTURE.md).
