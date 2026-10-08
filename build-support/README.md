# Controller build on macOS ARM64

[Polski](README.pl.md) | English | [Project README](../README.md)

From the repository root:

```sh
python3 scripts/b0-prepare.py
python3 scripts/b0-build.py
python3 scripts/test-host-udp.py
python3 scripts/test-host-udp.py --sanitize
```

Requirements: macOS ARM64, Python ≥3.9, Git, GitHub CLI (`gh`) with access to
public releases, BSD tar, `make` and available `/usr/bin/clang` with the macOS
SDK. Initial preparation needs network access; subsequent preparation reuses
downloaded archives and checkouts. Compilation uses local dependencies.

[b0-lock.json](b0-lock.json) pins versions, archive SHA-256 values and library
commits. Tools and dependencies go to ignored `.arduino/`. The scripts do not
install global tools.

- [ESP8266 core 3.1.2](https://github.com/esp8266/Arduino/releases/tag/3.1.2)
  uses GCC 10.3. The native ARM64 package is
  [ESPHome 10.3.0-esphome.2](https://github.com/esphome-libs/xtensa-lx106-elf-toolchain/releases/tag/10.3.0-esphome.2).
  The tool directory name matches the core dependency;
  the actual release and package checksum are explicit in the manifest.
- Arduino CLI 1.3.1 is pinned. ctags 5.8-arduino11 is built from
  [official Arduino sources](https://github.com/arduino/ctags/releases/tag/5.8-arduino11)
  with Clang. Preparation renames the `__unused__` macro
  that conflicts with the macOS SDK; it does not change the ctags parser.
- ArduinoJson 5.13.5, PubSubClient 2.7, NTPClient 3.2.0, Time 1.5 and vendored
  LiquidCrystal_I2C 1.1.4 retain their versions. The verified Time checkout
  stays in `.arduino/sources/Time`; its compilation copy omits the obsolete
  `Time.h` wrapper, which conflicts with system `time.h` on macOS.
  Sources use `TimeLib.h`; copies of `Time.cpp` and `DateStrings.cpp`
  also use that header instead of the wrapper. Library logic is unchanged.
- The core's Python launcher selects the interpreter used during preparation.
  Its path and version are recorded in `.arduino/prepared.json`. Changing
  the manifest requires preparation again before a build.

The build copies tracked controller files into `build/b0/sketch/heating_server`.
By default it uses only credential and relay examples and does not read local
credentials. `--local-config` explicitly selects the ignored local
`heating_server/secrets.h` and `relay_config.h` for an image using the owner's
configuration described in the [README](../README.md).

The script performs a clean compilation, saving the command in
`build/b0/command.json`, the log and memory sizes in `build/b0/compile.log`,
and ELF/BIN in `build/b0/output/`. It passes `MQTT_SOCKET_TIMEOUT=1` to the
entire sketch and PubSubClient; the library remains at 2.7. This is an MQTT
response timeout, not a thermostat-loss timeout. All module tests are described
in the [README](../README.md), with UDP details in
[tests/host-udp/README.md](../tests/host-udp/README.md).

The generic profile, including 512 KB flash, comes from the existing
compilation configuration and does not establish the physical board model.
This target prepares a firmware image; filesystem-image and upload tools
are not installed. It does not upload firmware or test physical outputs.
Thermostat sources are not part of this target.

## Communication compatibility

Controller 0.3.0 compiles HEARTBEAT reception and the 180 s timeout. The client
contract requires a heartbeat every 60 s, including without heating, and
`serial` matching `HEATING_THERMOSTAT_SERIALS` in local `relay_config.h`.
When updating earlier configuration, add this array and preserve other settings.
The default build uses zero assignments and cannot start CO.
No thermostat firmware target is available; its build, heartbeat transmission
and reconnect behavior remain BLOCKED because sources are absent. The update
procedure and requirements for both sides are in the [README](../README.md).
