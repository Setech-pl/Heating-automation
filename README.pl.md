# Heating Automation

Polski | [English](README.md)

[![CI](https://github.com/Setech-pl/Heating-automation/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Setech-pl/Heating-automation/actions/workflows/ci.yml)

Heating Automation to centrala ESP8266 obsługująca cztery pompy obiegowe
centralnego ogrzewania (CO, ID 1–4) i jedną pompę obiegową ciepłej wody
użytkowej (CWU, ID 5). Termostaty przekazują temperaturę pomieszczenia i żądania
grzania przez Wi-Fi/UDP. Firmware centrali znajduje się w
[heating_server/](heating_server/). W repozytorium nie ma zidentyfikowanego punktu
wejścia ani przepisu kompilacji firmware termostatów; rootowe projekty Visual
Studio są osobnymi programami hostowymi. Sprzęt i czujniki termostatów nie są
tutaj określone.

Centrala waliduje żądania, stosuje ograniczenia temperatur i przełączeń oraz
zapisuje skonfigurowane wyjścia przekaźników. Korzysta z LCD I2C, synchronizacji
NTP i połączenia MQTT z obsługą subskrypcji. Przychodzące wiadomości MQTT
nie sterują pompami, a telemetria nie jest publikowana.

Centrala 0.3.0 przyjmuje HEARTBEAT i po **>= 180 s** bez poprawnego komunikatu
przypisanego termostatu wyłącza wyłącznie jego pompę CO. Heartbeat nie włącza
pompy; po timeoutcie potrzebne jest nowe ON ze zwykłymi ograniczeniami.
Oczekujące stare ON są unieważniane. Kontrakt wymaga heartbeat co **60 s**,
także bez grzania; wysyłania i reconnectu termostatów nie można tu potwierdzić,
ponieważ brak ich źródeł. Mapowanie, warunki i protokół opisuje
[architektura z diagramami komponentów i protokołem](docs/ARCHITECTURE.pl.md).

## Kompilacja na macOS ARM64

Wymagania: macOS ARM64, Python ≥3.9, Git, GitHub CLI `gh`, `make`, BSD tar
oraz Clang z macOS SDK. Narzędzia i przypięte zależności instalują się lokalnie
w ignorowanym `.arduino/`, bez zmiany globalnego toolchaina.
Uruchom w katalogu głównym repozytorium:

```sh
python3 scripts/b0-prepare.py
python3 scripts/b0-build.py
```

Pierwsze przygotowanie wymaga dostępu do publicznych wydań GitHub. Kolejne
kompilacje używają lokalnych zależności. Wersje i dostosowania opisano w
[instrukcji kompilacji](build-support/README.pl.md) i
[b0-lock.json](build-support/b0-lock.json).

Domyślna kompilacja używa przykładowych danych logowania i nieskonfigurowanych
przekaźników. ELF/BIN trafiają do `build/b0/output/`, a log i rozmiary pamięci do
`build/b0/compile.log`. Skrypt nie wgrywa firmware. Profil generic ESP8266 z flash
512 KB jest profilem kompilacji, nie potwierdzeniem fizycznej płytki ani jej flash.

## Konfiguracja i uruchomienie

Utwórz lokalną konfigurację tylko wtedy, gdy pliki jeszcze nie istnieją:

```sh
cp -n heating_server/secrets.example.h heating_server/secrets.h
cp -n heating_server/relay_config.example.h heating_server/relay_config.h
```

Ustaw własne Wi-Fi, fallback AP i opcje MQTT, używając nazw z
`secrets.example.h`. Dane logowania przechowuj w lokalnym `secrets.h`;
zarówno `secrets.h`, jak i `relay_config.h` są ignorowane przez Git.
Te lokalne pliki są używane wyłącznie po jawnym wyborze:

```sh
python3 scripts/b0-build.py --local-config
```

`HEATING_RELAYS` zawiera pięć wierszy: ID pomp CO **1–4**, następnie ID pompy
CWU **5**. Każdy wiersz to **numer GPIO ESP8266** i poziom aktywny (`0` lub `1`),
nie etykieta Dx płytki ani ID pompy. Przykład zawiera `{-1, -1}` dla każdego
kanału. Repozytorium nie potwierdza fizycznej mapy pinów ani polaryzacji
przekaźników. Dopasuj konfigurację do płytki i okablowania.

`HEATING_THERMOSTAT_SERIALS` zawiera cztery numery seryjne chipów dla CO ID 1–4.
Wpis 0 wyłącza obsługę danego ID; dodatnie wpisy muszą być unikalne. Użyj
rzeczywistych numerów seryjnych przypisanych termostatów. Dodaj tę tablicę
z aktualnego przykładu do istniejącego lokalnego pliku, zachowując GPIO,
polaryzację oraz pozostałe ustawienia. Komunikaty nie zmieniają przypisania.

Driver odrzuca duplikaty GPIO, nieustaloną polaryzację, numery spoza 0–16,
piny flash 6–11, Serial 1/3 i domyślne piny Wire używane przez LCD
(4/5 w tym profilu kompilacji). Nie ma implementacji ekspandera przekaźników.
Skonfigurowane wyjścia startują OFF: latch jest ustawiany przed włączeniem
OUTPUT. Kanały nieskonfigurowane lub niezainicjalizowane odrzucają przełączenie.
Zachowanie GPIO przed startem firmware, obwody bootstrap, polaryzacja
przekaźników i styki NO/NC wymagają weryfikacji fizycznej. Programowy stan
wyjścia nie potwierdza położenia styków ani pracy pompy.

`HEATING_ENABLE_DOMESTIC_PLAN` domyślnie wynosi `false`. Włączenie opcji
rejestruje harmonogram CWU: ON o 05:00, 06:00, 07:00, 12:00, 16:00, 19:00
i 21:00, z OFF w harmonogramie 30 minut później. Niezależny limit pracy CWU
**15 minut** ma pierwszeństwo. Harmonogram świąteczny nie jest zdefiniowany.

Przy starcie firmware inicjalizuje wyjścia przekaźników i LCD, próbuje połączyć
się z zewnętrznym Wi-Fi, a po nieudanych próbach startowych próbuje włączyć AP.
Następnie próbuje synchronizacji NTP w trybie zewnętrznego Wi-Fi, uruchamia discovery UDP
i obsługę MQTT oraz przechodzi do `loop()`. Skrypty kompilacji nie zawierają
procedury wgrywania dostosowanej do instalacji. Utrata Wi-Fi podczas normalnej
pracy nie uruchamia procedury startowego AP; brak kontaktu termostatów uruchamia
niezależne timeouty CO po 180 s.

## Weryfikacja

Po przygotowaniu zależności dostępne są następujące kontrole hostowe:

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

Targety kompilują produkcyjną logikę z deterministycznym zegarem i atrapami GPIO,
LCD oraz transportów. Runtime obejmuje także rzeczywiste `setup()` i `loop()`.
NTP, parser JSON i kontrola timeoutu MQTT używają przypiętych bibliotek.
ASan/UBSan zatrzymują się po pierwszym błędzie; LSan na macOS jest wyłączony.
Testy hostowe nie potwierdzają fizycznego zachowania GPIO. Szczegóły:
[UDP](tests/host-udp/README.pl.md), [start](tests/host-startup/README.pl.md).

W osobno autoryzowanym sprawdzeniu sprzętu porównaj komunikaty Serial (115200),
stan sieci/czasu na LCD i odpowiedzi UDP `SHOWSTATUS` z obserwacją przekaźników
i pomp. `RUNNING` jest stanem programowym, a `OUTPUT` ostatnim zadanym stanem
wyjścia; żadne z nich nie jest sprzętowym sprzężeniem zwrotnym. Udana kompilacja
lub ACK nie potwierdzają fizycznego działania wyłączenia po utracie komunikacji.

## Aktualizacja urządzeń i zgodność

Centrala 0.3.0 wymaga lokalnego przypisania numerów seryjnych i klientów
wysyłających HEARTBEAT co 60 s oraz `serial` w ON/OFF. Starszy klient bez
tego pola nie może włączyć CO; klient wysyłający tylko ON przy zmianie stanu
straci kontakt po 180 s. Nie instaluj tej centrali z niezweryfikowanym klientem.
Źródła, target kompilacji i obraz termostatu nie są dostępne w repozytorium;
nie ma podstaw do podania komendy jego aktualizacji ani potwierdzenia reconnectu.

Przy aktualizacji zachowaj lokalną konfigurację, dodaj przypisania, przygotuj
zgodne obrazy obu stron i wyłącz instalację przed zmianą wersji. Zbuduj centralę
przez `python3 scripts/b0-build.py --local-config`. Obraz znajduje się
w `build/b0/output/heating_server.ino.bin`; wgranie wymaga procedury właściwej
dla potwierdzonej płytki i odbywa się osobno. Wgraj zgodny firmware termostatów
ich właściwym narzędziem, a następnie centralę. Po restarcie sprawdź OFF,
heartbeat, nowe ON oraz wyłączenie wyłącznie przypisanego CO po 180 s ciszy.
Nie wykonano wgrywania ani testów sprzętowych; brak firmware klienta blokuje
potwierdzenie zgodności całej instalacji.

## CI i zawartość repozytorium

[GitHub Actions](.github/workflows/ci.yml) uruchamia się na push, pull request
i `workflow_dispatch`. Job na macOS 15 ARM64 przygotowuje przypięte zależności,
uruchamia cztery targety hostowe normalnie i z ASan/UBSan, następnie kompiluje
centralę z przykładową konfiguracją. Podsumowanie zapisuje wyniki targetów,
zużycie RAM/IRAM/flash i rozmiar BIN; log runnera to `build/b0/compile.log`.
To opis workflow, a nie wynik konkretnego wykonania.

CI używa `contents: read`, akcji przypiętych pełnymi SHA i tokenu workflow
jako `GH_TOKEN` do publicznych wydań. Nie wymaga PAT ani danych logowania
właściciela i nie wgrywa firmware. Wygenerowane BIN/ELF, pliki Visual Studio
`.VC.db` i `.vcxproj.user` są ignorowane. Windowsowe `.sln`, `.vcxproj`, ich
filtry i rootowe źródła pozostają oddzielone od kompilacji centrali.
