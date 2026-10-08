# Hostowe testy parsera UDP

Polski | [English](README.md) | [README projektu](../../README.pl.md)

Z katalogu głównego produktu, na macOS z Pythonem 3 i Clang:

```sh
python3 scripts/test-host-udp.py --arduino-json /path/to/ArduinoJson
python3 scripts/test-host-udp.py --arduino-json /path/to/ArduinoJson --sanitize
```

Argument wskazuje katalog biblioteki zawierający `src/ArduinoJson.h` i `.git`.
Jeśli istnieje projektowe `.arduino/user/libraries/ArduinoJson`, argument można
pominąć. Przy braku źródeł można pobrać wyłącznie tę bibliotekę do ignorowanego katalogu:

```sh
git clone --depth 1 --branch v5.13.5 https://github.com/bblanchon/ArduinoJson.git .arduino/user/libraries/ArduinoJson
```

Target sprawdza wersję 5.13.5, commit
`ad4b13c8f044e67f1610fba96e8dc108ebc90cd5` i czysty checkout według
`build-support/b0-lock.json`. Nie pobiera zależności ani nie przygotowuje
toolchaina firmware. Błąd kompilacji, niezgodna zależność albo nieudana asercja
kończy uruchomienie kodem niezerowym. Komendy kompilacji są wypisywane; binaria
powstają w ignorowanym `build/host-udp/{normal,sanitize}`. `CXX` pozwala wskazać
inny dostępny Clang.

Testy kompilują bezpośrednio `heating_server/udpmessengerservice.cpp` oraz
rzeczywisty ArduinoJson. Adaptery zastępują wyłącznie UDP, Wi-Fi, Serial, ESP
i zegar odpowiedzi. Nie ma kopii parsera, JSON ani tworzenia komendy w stubach.
Odbiorca testowy liczy udostępnione komendy i próby przekazania ON/OFF; nie
wykonuje controllera, schedulera ani GPIO. Test sprawdza też zachowanie całego
stanu komendy i adresu odpowiedzi po odrzuceniu datagramu.

Kompilacja używa C++11, bez RTTI i wyjątków; ustawienia ArduinoJson zachowują
`float`, wyłączone `long long` i limit zagnieżdżenia 10 jak na ESP8266. Historyczne nagłówki biblioteki
są traktowane jako systemowe, a ostrzeżenie macOS o istniejącym `sprintf` jest
wyłączone; pozostałe `-Wall -Wextra` są błędami. W trybie sanitizerów działają
ASan i UBSan z zatrzymaniem po pierwszym błędzie. LeakSanitizer jest wyłączony
na macOS. Rozmiary wskaźników i puli JSON hosta różnią się od ESP8266.

Parser przyjmuje pojedynczy kompletny obiekt JSON o długości 1–512 B,
bez NUL, z opcjonalnymi białymi znakami JSON wokół. Wszystkie komendy
wymagają pól `cmd`, `ID`, `actualTEMP` i `targetTEMP`:
ON, OFF, SHOWSERVER, SHOWSTATUS oraz HEARTBEAT. `cmd` jest dokładną nazwą
krótszą niż 20 B. `ID` to pełna liczba całkowita w zakresie `int`,
a temperatury to pełne liczby dziesiętne mieszczące się w skończonym `float`
bez błędu zakresu konwersji. Liczby JSON i ciągi liczbowe pozostają obsługiwane.

HEARTBEAT wymaga ID 1–4 i pola `serial`. Podane `serial` we wszystkich komendach
musi być dodatnią liczbą całkowitą 1–4294967295 lub tekstem złożonym z 1–10 cyfr;
wartość jest kopiowana do własnej pamięci komendy. Testy sprawdzają brak pola,
zero, przepełnienie, długość, typy, ułamki i niepoprawny tekst. Starsze ON/OFF
bez `serial` są parsowane, ale router odrzuca je bez odnowienia kontaktu.
Powiązanie numeru seryjnego i obiegu sprawdza runtime, nie sam parser.
ON/OFF dopuszcza ID 1–4 w routerze, a SHOWSTATUS 1–5. `versionC`, wilgotność
i inne dodatkowe pola są ignorowane; muszą nadal być poprawnym JSON i mieścić
się w pojemności biblioteki. Komentarze, pojedyncze cudzysłowy, klucze bez
cudzysłowów, niepełne tokeny i dane po obiekcie są odrzucane. Sekwencje
`\uXXXX` są odrzucane, bo przypięty ArduinoJson 5.13.5 ich nie dekoduje.
Głębokość ogranicza biblioteka. Walidator składni nie tworzy danych komendy
ani nie zastępuje dekodowania biblioteki.

Odrzucenie zachowuje ostatnią poprawną komendę, jej flagę i adres odpowiedzi.
Nie tworzy nowej publikacji ani skutku sterującego. Komenda ma własną pamięć;
nie przechowuje wskaźników do datagramu ani lokalnej puli JSON.

Ten target nie potwierdza builda firmware, budżetu flash/RAM, działania pełnego
routera, polityki pomp ani zachowania fizycznych wyjść.
Zachowanie przy utracie termostatu opisuje [architektura](../../docs/ARCHITECTURE.pl.md).
