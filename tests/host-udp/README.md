# Hostowe testy parsera UDP

Z katalogu głównego produktu, na macOS z Pythonem 3 i Clang:

```sh
python3 scripts/test-host-udp.py --arduino-json /ścieżka/do/ArduinoJson
python3 scripts/test-host-udp.py --arduino-json /ścieżka/do/ArduinoJson --sanitize
```

Argument wskazuje katalog biblioteki zawierający `src/ArduinoJson.h` i `.git`.
Jeśli istnieje projektowe `.arduino/user/libraries/ArduinoJson`, argument można
pominąć. Można użyć wcześniej przygotowanego checkoutu biblioteki:

```sh
python3 scripts/test-host-udp.py --arduino-json ../checkout/.arduino/user/libraries/ArduinoJson
python3 scripts/test-host-udp.py --arduino-json ../checkout/.arduino/user/libraries/ArduinoJson --sanitize
```

Przy braku źródeł można pobrać wyłącznie tę bibliotekę do ignorowanego katalogu:

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

Parser przyjmuje jeden kompletny obiekt JSON o długości 1–512 B, bez NUL
w datagramie, z opcjonalnymi spacjami JSON przed i po obiekcie. Wymagane są
`cmd`, `ID`, `actualTEMP`, `targetTEMP` dla wszystkich czterech obsługiwanych
komend: `ON`, `OFF`, `SHOWSERVER`, `SHOWSTATUS`. `cmd` musi być dokładną nazwą
komendy, krótszą od 20 B. ID jest pełną liczbą całkowitą mieszczącą się w `int`;
temperatury są pełnymi liczbami dziesiętnymi reprezentowalnymi jako skończony
`float`, bez błędu zakresu konwersji. Liczby JSON i tekst liczbowy pozostają
obsługiwane. ID nie jest obcinane do prefiksu, a wartości temperatur nie mogą
być NaN ani nieskończonością.

Nie dodano zakresów fizycznych temperatur ani nowego powiązania ID z pompą
czy nadawcą. Przykład w nagłówku zawiera ID 11; istniejący controller osobno
dopuszcza pompy CO 1–4, a status konfiguracji obejmuje 1–5. Te domeny nie są
nową walidacją protokołu w parserze. Pola `serial`, `versionC`, wilgotność
i inne dodatkowe pola pozostają niewymagane i nie trafiają do komendy.
Ich treść musi mieć poprawną składnię JSON i zmieścić się w puli biblioteki.
Komentarze, pojedyncze cudzysłowy, niecytowane klucze, niepełne tokeny i dane
za obiektem są odrzucane. Sekwencje `\uXXXX` są odrzucane, ponieważ przypięty
ArduinoJson 5.13.5 ich nie dekoduje. Głębokość obiektów/tablic jest ograniczona
do limitu tej biblioteki. Walidator składni przed ArduinoJson nie tworzy danych
komendy ani nie zastępuje dekodowania biblioteki.

Odrzucenie zachowuje ostatnią poprawną komendę, jej flagę i adres odpowiedzi.
Nie tworzy nowej publikacji ani skutku sterującego. Komenda ma własną pamięć;
nie przechowuje wskaźników do datagramu ani lokalnej puli JSON.

Ten target nie potwierdza builda firmware, budżetu flash/RAM, działania pełnego
routera, polityki pomp ani zachowania fizycznych wyjść.
