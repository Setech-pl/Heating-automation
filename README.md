# Heating Automation

Centrala ESP8266 odbiera żądania termostatów przez UDP i steruje czterema
pompami CO oraz pompą obiegową CWU. Kod centrali znajduje się w
`heating_server/`. Źródła i build firmware termostatów nie zostały odnalezione;
rootowe projekty Windows są osobnymi, historycznymi programami hostowymi.

## Build na Macu

Wymagane: macOS ARM64, Python ≥3.9, Git, GitHub CLI `gh`, `make`, BSD tar
oraz Clang z macOS SDK. Narzędzia i przypięte zależności instalują się lokalnie
w ignorowanym `.arduino/`, bez zmiany globalnego toolchaina.

```sh
python3 scripts/b0-prepare.py
python3 scripts/b0-build.py
```

Pierwsze przygotowanie wymaga dostępu do publicznych wydań GitHub. Kolejne
buildy używają lokalnych zależności. Manifest i szczegóły wersji opisano w
[build-support/README.md](build-support/README.md).

Domyślny build używa wyłącznie przykładowych sekretów i nieskonfigurowanych
przekaźników. ELF/BIN trafiają do `build/b0/output/`, a log i rozmiary do
`build/b0/compile.log`. Skrypt nie flashuje urządzenia. Profil generic ESP8266
z flash 512 KB jest historycznym profilem kompilacji; trzeba porównać go
z rzeczywistym modułem przed przygotowaniem obrazu dla płytki.

## Testy

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

Testy kompilują produkcyjny kod centrali. Target runtime obejmuje również
rzeczywiste `setup()` i `loop()`. Adaptery zapewniają kontrolowany zegar,
GPIO, LCD i transport, bez urządzeń oraz rzeczywistych połączeń sieciowych.
NTP, parser JSON i test timeoutu MQTT korzystają z przypiętych bibliotek. ASan/UBSan zatrzymują
się po pierwszym błędzie; LSan na macOS jest wyłączony. Bilans destrukcji
zadań jest sprawdzany osobno. Testy hostowe nie potwierdzają działania płytki.

## Konfiguracja

Jeżeli nie masz jeszcze własnych plików, skopiuj przykłady bez nadpisywania
istniejącej konfiguracji:

```sh
cp -n heating_server/secrets.example.h heating_server/secrets.h
cp -n heating_server/relay_config.example.h heating_server/relay_config.h
```

W `secrets.h` wpisz własne Wi-Fi, dane fallback AP oraz adres i dane logowania
brokera MQTT. Nazwy opcji są w przykładzie. Pliki `secrets.h` i
`relay_config.h` pozostają lokalne i ignorowane przez Git. Obraz korzystający
z tych plików buduje się wyłącznie po jawnym wyborze:

```sh
python3 scripts/b0-build.py --local-config
```

Tabela `HEATING_RELAYS` ma pięć wierszy, kolejno dla ID pomp **1–4 CO i 5 CWU**.
Każdy wiersz zawiera numer **GPIO ESP8266** i poziom aktywny: `0` lub `1`.
To nie są etykiety Dx płytki ani identyfikatory pomp. Przykład pozostawia obie
wartości jako `-1`: repozytorium nie zawiera potwierdzonego przypisania ani
polaryzacji. Uzupełnij je z dokumentacji posiadanego modułu i okablowania.

Driver odrzuca duplikaty GPIO, nieustaloną polaryzację, numery spoza 0–16,
piny flash 6–11, Serial 1/3 oraz domyślne piny Wire używane przez LCD
(w obecnym profilu 4/5). Nie ma implementacji ekspandera przekaźników.
Skonfigurowane wyjścia startują OFF: najpierw ustawiany jest latch, potem tryb
OUTPUT. Nieustalone lub nieuruchomione kanały odrzucają przełączenie.
Stany pinów przed uruchomieniem firmware, obwody bootstrap, fizyczna
polaryzacja i styki NO/NC wymagają sprawdzenia na sprzęcie.

`HEATING_ENABLE_DOMESTIC_PLAN` domyślnie wynosi `false`. Opcja włącza istniejący
plan CWU: start o 05:00, 06:00, 07:00, 12:00, 16:00, 19:00 i 21:00 oraz
planowane OFF po 30 minutach. Niezależny limit CWU **15 minut** ma pierwszeństwo
nad dłuższym oknem. Plan świąteczny nie jest zdefiniowany.

## Dostępne działanie

- UDP na porcie **3636**: `ON`, `OFF`, `SHOWSTATUS`, `SHOWSERVER`.
  Wymagane pola JSON to `cmd`, `ID`, `actualTEMP`, `targetTEMP`; wartości
  liczbowe mogą być liczbami JSON lub pełnymi ciągami liczbowymi.
  ON/OFF dotyczą ID 1–4, status ID 1–5. SHOWSERVER uruchamia discovery,
  nie rejestruje klienta. Niepoprawne pakiety nie zmieniają stanu.
- ACK zachowuje `cmd=OK/NO`, `RUNNING=YES/NO`, `TIME` i `SERVERIP`.
  Dodano `OUTPUT=ON/OFF/UNCONFIGURED`. OK dla przełączenia oznacza udany
  zapis skonfigurowanego wyjścia; RUNNING opisuje stan programowy po komendzie,
  a OUTPUT ostatni znany stan wyjścia. Żadne pole nie jest pomiarem pracy
  pompy ani potwierdzeniem zamknięcia styku. Powtórne ON dla działającej pompy
  i OFF dla wyłączonej nadal zwracają NO.
- Czas pracy, minimalny czas ON i przerwa OFF są monotoniczne, odporne na
  pojedyncze przepełnienie 32-bitowego `millis()` i skoki NTP. Zachowane limity:
  CO **7720 minut**, CWU **15 minut**, minimum ON/OFF **1 minuta**.
  Kontrola działa co sekundę także bez NTP i nie wymaga wolnego slotu schedulera.
  Błąd OFF nie zeruje stanu ani licznika; następna kontrola ponawia próbę.
- Modyfikator dzienny obejmuje godziny 11–13, nocny 23–05. Końce zachowują
  dotychczasowe warunki: 22 i 06 są poza nocą. Bez ustawionego zegara
  modyfikatory kalendarzowe są pomijane; podstawowe progi temperatur pozostają.
- Scheduler obsługuje minutowe, godzinowe, dzienne, tygodniowe i miesięczne
  zadania. Dni tygodnia mają zakres TimeLib 1–7. Miesięczne zadania pomijają
  miesiąc bez wskazanego dnia. Pominięte terminy nie są odtwarzane; zadanie
  może wykonać się raz w bieżącej zaplanowanej minucie, potem dostaje termin
  w przyszłości. Cofnięcie NTP nie powtarza już wykonanych terminów.
  Nieustawiony zegar wstrzymuje zadania kalendarzowe.
- LCD I2C 20×4 pod adresem 0x27; synchronizacja NTP z zachowanym przesunięciem
  UTC+1, bez automatycznej zmiany czasu letniego. Gdy zewnętrzne Wi-Fi zawiedzie
  na starcie, centrala uruchamia AP. Discovery i odpowiedzi używają adresu AP.
- MQTT obsługuje połączenie, dane logowania i subskrypcję. Reconnect wykonuje
  jedną próbę na minutę; build ustawia timeout odpowiedzi MQTT na 1 sekundę,
  a WiFiClient timeout DNS/TCP na 200 ms. Transport nadal jest synchroniczny;
  rzeczywiste opóźnienia wymagają pomiaru na płytce. Schemat komend MQTT oraz
  telemetria nie są zdefiniowane: odbiór komunikatu nie wykonuje sterowania.

Pozostają do ustalenia: model i flash płytki, mapa GPIO/polaryzacja, zachowanie
przy boot/reset i potwierdzenie wyjść, firmware i czujniki termostatów, polityka
utraty termostatu/sieci oraz kontrakty rejestracji i sterowania MQTT. UDP nie ma
uwierzytelniania ani powiązania ID z nadawcą. Priorytety pomp są w konfiguracji,
ale nie definiują obecnie arbitrażu ani limitu jednoczesnych obiegów.
