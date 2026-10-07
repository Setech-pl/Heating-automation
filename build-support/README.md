# Kompilacja centrali na macOS ARM64

W katalogu głównym produktu:

```sh
python3 scripts/b0-prepare.py
python3 scripts/b0-build.py
python3 scripts/test-host-udp.py
python3 scripts/test-host-udp.py --sanitize
```

Wymagania: macOS ARM64, Python ≥3.9, Git, GitHub CLI (`gh`) z dostępem do
publicznych wydań, BSD tar, `make` i dostępny `/usr/bin/clang` z macOS SDK.
Pierwsze przygotowanie wymaga sieci; kolejne używa pobranych archiwów
i checkoutów. Kompilacja korzysta z lokalnych zależności.

Manifest [b0-lock.json](b0-lock.json) przypina wersje, SHA-256 archiwów
i commity bibliotek. Narzędzia i zależności trafiają do ignorowanego
`.arduino/`. Skrypty nie instalują globalnych narzędzi.

- [ESP8266 core 3.1.2](https://github.com/esp8266/Arduino/releases/tag/3.1.2)
  korzysta z GCC 10.3. Natywny pakiet ARM64 pochodzi z
  [ESPHome 10.3.0-esphome.2](https://github.com/esphome-libs/xtensa-lx106-elf-toolchain/releases/tag/10.3.0-esphome.2).
  Nazwa katalogu narzędzia odpowiada zależności core, a rzeczywiste wydanie
  i suma pakietu są jawne w manifeście.
- Arduino CLI 1.3.1 pozostaje przypięte. ctags 5.8-arduino11 jest budowany
  z [oficjalnych źródeł Arduino](https://github.com/arduino/ctags/releases/tag/5.8-arduino11)
  przez Clang. Skrypt zmienia nazwę makra `__unused__`, które koliduje
  z obecnym macOS SDK; nie zmienia parsera ctags.
- ArduinoJson 5.13.5, PubSubClient 2.7, NTPClient 3.2.0, Time 1.5 i vendored
  LiquidCrystal_I2C 1.1.4 zachowują wersje. Zweryfikowany checkout Time
  pozostaje w `.arduino/sources/Time`; jego kopia do kompilacji pomija tylko
  przestarzały wrapper `Time.h`, kolidujący z systemowym `time.h` na macOS.
  Źródła korzystają z `TimeLib.h`; kopie `Time.cpp` i `DateStrings.cpp` również
  używają tej nazwy zamiast wrappera. Logika biblioteki nie jest zmieniana.
- Launcher Python core wskazuje istniejący interpreter użyty do przygotowania.
  Jego ścieżka i wersja są zapisane w `.arduino/prepared.json`. Zmiana manifestu
  wymaga ponownego przygotowania przed buildem.

Build kopiuje śledzone pliki centrali do `build/b0/sketch/heating_server`
i używa wyłącznie `heating_server/secrets.example.h` jako `secrets.h`.
Nie czyta lokalnych sekretów. Wykonuje czystą kompilację, zapisując komendę
w `build/b0/command.json`, log wraz z rozmiarami w `build/b0/compile.log`,
a ELF/BIN w `build/b0/output/`. Testy UDP opisano w
[tests/host-udp/README.md](../tests/host-udp/README.md).

Opcje profilu generic, w tym 512 KB flash, pozostają historyczne i nie
potwierdzają konfiguracji płytki. Ten target przygotowuje obraz firmware;
narzędzia obrazów systemów plików i uploadu nie są instalowane. Nie flashuje
sprzętu ani nie testuje fizycznych wyjść. Źródła termostatów nie są częścią
tego targetu.
