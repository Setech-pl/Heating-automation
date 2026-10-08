# Heating Automation

[Polski](README.pl.md) | English

[![CI](https://github.com/Setech-pl/Heating-automation/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Setech-pl/Heating-automation/actions/workflows/ci.yml)

Heating Automation is an ESP8266 central controller for four space-heating
circulation pumps (CO, IDs 1–4) and one domestic hot-water circulation pump
(CWU, ID 5). Thermostats provide room temperature and heating requests over
Wi-Fi/UDP. Controller firmware is in [heating_server/](heating_server/).
This repository contains no identified thermostat firmware entry point or
build recipe; its root-level Visual Studio projects are separate host programs.
Thermostat hardware and sensors are not specified here.

The controller validates requests, applies temperature and switching limits,
and writes configured relay outputs. It has an I2C LCD, NTP synchronization
and an MQTT connection with subscription support. Incoming MQTT messages do
not control pumps, and telemetry is not published.

Controller 0.3.0 accepts HEARTBEAT and, after **>= 180 s** without a valid
message from the assigned thermostat, switches OFF only its CO pump.
Heartbeat never starts a pump; after timeout a new ON must satisfy normal
gates. Pending old ON requests are invalidated. The contract requires a
heartbeat every **60 s**, including without heating; thermostat transmission
and reconnect cannot be established here because their sources are absent.
See the [architecture, component diagrams and protocol](docs/ARCHITECTURE.md).

## Build on macOS ARM64

Requirements: macOS ARM64, Python ≥3.9, Git, GitHub CLI `gh`, `make`, BSD tar
and Clang with the macOS SDK. Tools and pinned dependencies are installed
locally in ignored `.arduino/`, without changing the global toolchain.
Run from the repository root:

```sh
python3 scripts/b0-prepare.py
python3 scripts/b0-build.py
```

Initial preparation needs access to public GitHub releases. Subsequent builds
use local dependencies. Versions and adaptations are described in the
[build guide](build-support/README.md) and [b0-lock.json](build-support/b0-lock.json).

The default build uses dummy credentials and unconfigured relays. ELF/BIN files
go to `build/b0/output/`, and the log and memory sizes to `build/b0/compile.log`.
The script does not upload firmware. The generic ESP8266 profile with 512 KB
flash is a compilation profile, not confirmation of the physical board or flash.

## Configuration and startup

Create local configuration only if the files do not already exist:

```sh
cp -n heating_server/secrets.example.h heating_server/secrets.h
cp -n heating_server/relay_config.example.h heating_server/relay_config.h
```

Set your Wi-Fi, fallback AP and MQTT options using the names in
`secrets.example.h`. Keep credentials in local `secrets.h`; both `secrets.h`
and `relay_config.h` are ignored by Git. These local files are used only
when explicitly selected:

```sh
python3 scripts/b0-build.py --local-config
```

`HEATING_RELAYS` contains five rows: CO pump IDs **1–4**, then CWU pump ID **5**.
Each row is an **ESP8266 GPIO number** and an active level (`0` or `1`),
not a Dx board label or pump ID. The example uses `{-1, -1}` for every channel.
No physical pin mapping or relay polarity is confirmed by this repository.
Match configuration to the actual board and wiring.

`HEATING_THERMOSTAT_SERIALS` contains four chip serials for CO IDs 1–4.
Entry 0 disables that ID; positive entries must be unique. Use the actual
serials of the assigned thermostats. Add this array from the current example
to an existing local file while preserving GPIO, polarity and other settings.
Packets cannot change the assignment.

The driver rejects duplicate GPIOs, unknown polarity, numbers outside 0–16,
flash pins 6–11, Serial pins 1/3 and the default Wire pins used by the LCD
(4/5 in this build profile). There is no relay expander implementation.
Configured outputs initialize OFF, setting the latch before enabling OUTPUT.
Unconfigured or unsuccessfully initialized channels reject switching.
GPIO behavior before firmware startup, bootstrap circuitry, relay polarity
and NO/NC contacts require physical verification. Software output status
does not confirm relay contact position or pump operation.

`HEATING_ENABLE_DOMESTIC_PLAN` defaults to `false`. Enabling it registers the
CWU schedule: ON at 05:00, 06:00, 07:00, 12:00, 16:00, 19:00 and 21:00,
with scheduled OFF 30 minutes later. The independent **15-minute** CWU runtime
limit takes precedence. No holiday schedule is defined.

At startup firmware initializes relay outputs and the LCD, attempts external
Wi-Fi, and attempts to enable an AP if those startup attempts fail. It then
attempts NTP synchronization in external Wi-Fi mode, starts UDP discovery and MQTT servicing,
and enters `loop()`. The build scripts do not provide an installation-specific
upload procedure. Loss of Wi-Fi during normal operation does not trigger the
startup AP procedure; missing thermostat contact triggers separate CO timeouts
after 180 s.

## Verification

After preparing dependencies, the available host checks are:

```sh
python3 scripts/test-host-udp.py
python3 scripts/test-host-startup.py
python3 scripts/test-host-scheduler.py
python3 scripts/test-host-runtime.py
python3 scripts/test-host-udp.py --sanitize
python3 scripts/test-host-startup.py --sanitize
python3 scripts/test-host-scheduler.py --sanitize
python3 scripts/test-host-runtime.py --sanitize
```

These targets compile production logic with deterministic clocks and fake GPIO,
LCD and transports. Runtime also exercises the actual `setup()` and `loop()`.
NTP, JSON parsing and the MQTT timeout check use pinned libraries. ASan/UBSan
stop at the first error; LSan is disabled on macOS. Host tests do not establish
physical GPIO behavior. Details: [UDP](tests/host-udp/README.md),
[startup](tests/host-startup/README.md).

For a separately authorized hardware check, compare Serial output (115200),
LCD network/time status and UDP `SHOWSTATUS` responses with observed relay and
pump operation. `RUNNING` is software state and `OUTPUT` is the last issued
output state; neither is hardware feedback. A successful build or ACK does
not establish physical operation of communication-loss shutdown.

## Device update and compatibility

Controller 0.3.0 requires local serial assignments and clients sending
HEARTBEAT every 60 s plus `serial` in ON/OFF. An older client without that
field cannot start CO; a client sending ON only on state changes loses contact
after 180 s. Do not install this controller with an unverified client.
Thermostat sources, build target and image are absent from this repository;
there is no basis for a client update command or confirmation of reconnect.

For an update, preserve local configuration, add assignments, prepare compatible
images for both sides and shut down the installation before changing versions.
Build the controller with `python3 scripts/b0-build.py --local-config`.
The image is `build/b0/output/heating_server.ino.bin`; uploading requires the
procedure for the confirmed board and is a separate operation. Upload compatible
thermostat firmware with its own tool, then the controller. After restart,
check OFF, heartbeat, new ON and shutdown of only the assigned CO after 180 s
of silence. No upload or hardware tests were performed; absent client firmware
blocks confirmation of compatibility across the installation.

## CI and repository contents

[GitHub Actions](.github/workflows/ci.yml) runs on push, pull request and
`workflow_dispatch`. The macOS 15 ARM64 job prepares pinned dependencies,
runs all four host targets normally and with ASan/UBSan, then builds the
controller with example configuration. Its summary records target results,
RAM/IRAM/flash usage and BIN size; the runner log is `build/b0/compile.log`.
This describes the workflow, not a result of a particular execution.

CI uses `contents: read`, full-SHA action pins and the workflow token as
`GH_TOKEN` for public releases. It needs no owner PAT or credentials and does
not upload firmware. Generated BIN/ELF, Visual Studio `.VC.db` and
`.vcxproj.user` files are ignored. Windows `.sln`, `.vcxproj`, their filters
and root sources remain separate from the controller build.
