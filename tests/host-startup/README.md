# Regresje MQTT, NTP i LCD centrali

Z katalogu głównego produktu, po przygotowaniu zależności:

```sh
python3 scripts/test-host-startup.py
python3 scripts/test-host-startup.py --sanitize
```

Wymagania: Python ≥3.9, Clang (`CXX` może wskazać inny Clang) i przypięty,
czysty checkout NTPClient 3.2.0 w `.arduino/user/libraries/NTPClient`.
Skrypt nie pobiera zależności. Pliki produkcyjne `utils`, `screen`, `scheduler`
i `heating_config` są każdorazowo kopiowane do ignorowanego
`build/host-startup/{normal,sanitize}/source`, tak jak przy buildzie firmware,
aby `secrets.h` wskazywało wyłącznie przykład. Nie ma osobnej implementacji
logiki w testach ani odczytu lokalnych sekretów.

Target wykonuje produkcyjne formatowanie tematu, ograniczoną synchronizację
startową, komendę NTP oraz render ekranu. Kompiluje rzeczywisty NTPClient:
fake UDP podaje pakiet NTP lub timeout. Adaptery zastępują tylko interfejsy
Arduino, Wi-Fi/UDP, zegar i LCD. Czas jest deterministyczny, `delay` przesuwa
licznik bez czekania. Nie ma połączeń sieciowych ani dostępu do sprzętu.

Regresje sprawdzają temat dla pomp 1–4 (domena istniejącego controllera),
bufor dokładnego rozmiaru i za mały, odrzucenie uciętego tematu i niepoprawnych
ID; sukces NTP na próbach 1/3/5, timeout i limit 5 prób, AP bez prób i bez
zmiany zegara; inicjalizację komunikatu komendy oraz pierwszy i niezmieniony
render na pamięci z różnymi wzorcami; tekst LCD długości 0/19/20/21/100,
graniczne IPv4 oraz wybór STA/AP i format IP w menu. LCD przycina i dopełnia
tekst do 20 znaków; formatowanie tematu MQTT zwraca błąd i pusty bufor
przy ucięciu. Wynik NTP rozróżnia pominięcie, synchronizację i błąd.

Tryb `--sanitize` wykonuje ASan/UBSan z zatrzymaniem po pierwszym błędzie;
LeakSanitizer jest wyłączony na macOS. ASan/UBSan nie wykrywają wszystkich
odczytów niezainicjalizowanych danych; wzorce pamięci i obserwacja renderu są
osobną regresją. To test modułów używanych przez szkic, nie pełnego `setup()`
ani routera/schedulera. Istniejące ostrzeżenia pozostałych modułów są widoczne.
Build firmware i regresje UDP należy uruchamiać oddzielnie.
