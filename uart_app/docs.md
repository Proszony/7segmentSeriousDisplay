# Dokumentacja Projektu UART 7-Segment Display

## Struktura katalogów

```
uart_app/
├── include/           # Pliki nagłówkowe (.h)
│   ├── framing.h      # Definicje struktur Frame i SegState
│   ├── video.h        # Deklaracje funkcji wideo
│   ├── cli_parser.h   # Definicje parametrów i parser CLI
│   ├── uart.h         # Interfejs klasy UART
│   └── audio.h        # Deklaracje funkcji audio
├── src/               # Pliki implementacji (.cpp)
│   ├── main.cpp       # Główna logika aplikacji
│   ├── framing.cpp    # Implementacja serializacji/deserializacji
│   ├── video.cpp     # Implementacja przetwarzania wideo
│   ├── cli_parser.cpp # Implementacja parsowania argumentów
│   ├── uart.cpp      # Implementacja komunikacji UART
│   └── audio.cpp     # Implementacja odtwarzania audio
├── Makefile          # Konfiguracja kompilacji
├── docs.md           # Dokumentacja projektu
└── app               # Skompilowany plik wykonywalny
```

---

## Tryby pracy

| Tryb | Komenda | Opis |
|------|---------|------|
| **send** (domyślny) | `./app -m send` | Wideo → przetwarzanie → serializacja → UART |
| **receive** | `./app -m receive` | UART → deserializacja → wyświetlanie/logowanie |
| **display** | `./app -m display` | Lokalne wyświetlanie wideo (bez UART) |

---

## Pliki nagłówkowe

### `include/framing.h`

**Opis:** Definiuje struktury danych do pakowania ramek oraz funkcje do serializacji/deserializacji.

**Struktury:**
- `SegState` - stan 7 segmentów (bool on[7])
- `Frame` - ramka danych z nagłówkiem, wymiarami, komórkami i bitem końca

**Stałe:**
- `FRAME_START = 0x2137` - marker początku ramki
- `FRAME_END = 0x69` - marker końca ramki

**Funkcje:**
- `create_frame()` - tworzy Frame z vector<SegState>
- `writeUint16LE()` - zapisuje uint16_t w little-endian
- `serialize()` - serializuje Frame do vector<uint8_t>
- `deserialize()` - deserializuje dane do Frame

---

### `include/video.h`

**Opis:** Deklaracje funkcji do przetwarzania wideo i wyświetlania ramek.

**Funkcje:**
- `run_video()` - przetwarza wideo i zapisuje stany segmentów do referencji vector<SegState>
- `display_frame()` - wyświetla otrzymaną ramkę w oknie OpenCV

---

### `include/cli_parser.h`

**Opis:** Definiuje strukturę parametrów i deklaracje funkcji parsowania CLI.

**Struktura `Params`:**
- `filename` - ścieżka do pliku wideo (domyślnie: `../res/BadApple!!.mp4`)
- `thresh` - próg binaryzacji (domyślnie: 0.5)
- `invert_flag` - czy invertować obraz
- `audio_flag` - czy odtwarzać audio
- `draw` - czy wyświetlać okno z wideo
- `max_fps` - maksymalna liczba klatek na sekundę (-1 = bez limitu)
- `res` - rozdzielczość wyjściowa (domyślnie: 640x360)
- `div` - liczba segmentów poziomo/pionowo (domyślnie: 32x12)
- `seg_color` - kolor segmentów w BGR (domyślnie: czerwony)
- `mode` - tryb pracy: "send", "receive", "display" (domyślnie: "send")
- `port` - ścieżka urządzenia UART (domyślnie: "/dev/ttyUSB0")
- `baudrate` - prędkość transmisji (domyślnie: 115200)

**Funkcje:**
- `parse_args()` - parsuje argumenty wiersza poleceń
- `print_help()` - wyświetla pomoc

---

### `include/uart.h`

**Opis:** Interfejs klasy do komunikacji przez port szeregowy (POSIX termios).

**Metody:**
- `open(port, baudrate)` - otwiera port z podaną prędkością
- `close()` - zamyka port
- `isOpen()` - sprawdza czy port jest otwarty
- `write(data)` - zapisuje dane do portu
- `read(count, timeout_ms)` - odczytuje dane z portu

---

### `include/audio.h`

**Opis:** Deklaracja funkcji do odtwarzania audio z pliku wideo.

**Funkcje:**
- `run_audio(filename, stop_flag)` - odtwarza audio w osobnym wątku

---

## Pliki implementacji

### `src/main.cpp`

**Opis:** Główna logika aplikacji - obsługuje trzy tryby pracy.

**Tryb "send" (domyślny):**
1. Otwiera plik wideo
2. Dla każdej klatki: przetwarza obraz → tworzy SegStates → tworzy Frame → serializuje → wysyła przez UART
3. Opcjonalnie odtwarza audio równolegle
4. Wyświetla okno z wideo jeśli flaga `-d`

**Tryb "receive":**
1. Nasłuchuje na porcie UART
2. Szuka markerów start/end ramki (0x2137 i 0x69)
3. Deserializuje dane i:
   - Jeśli flaga `-d`: wyświetla w oknie OpenCV
   - Bez flagi `-d`: loguje co 30 klatek do konsoli

**Tryb "display":**
1. Działa jak oryginalna symulacja - wyświetla wideo lokalnie bez UART

**Klawisze:**
- ESC - wyjście z pętli

---

### `src/framing.cpp`

**Opis:** Implementacja funkcji do tworzenia i serializacji ramek.

**Detale:**
- `create_frame()` konwertuje vector<SegState> do Frame, kodując 7 bitów segmentów w jeden bajt
- `serialize()` w Little-Endian: start(2B) + max_X(1B) + max_Y(1B) + cells + end(1B)
- `deserialize()` waliduje markery i rozmiar danych

---

### `src/video.cpp`

**Opis:** Implementacja przetwarzania wideo i wyświetlania.

**Detale:**
- `run_video()`: dzieli klatkę na komórki, dla każdej wyznacza 7 prostokątów segmentów, oblicza średnią jasność, binaryzuje
- `display_frame()`: rysuje otrzymaną ramkę w oknie OpenCV używając tych samych proporcji segmentów

**Parametry segmentów:**
- A (górny poziomy): 20%-80% szerokości, 5%-17% wysokości
- B (prawy górny): 78%-90% szerokości, 10%-50% wysokości
- C (prawy dolny): 78%-90% szerokości, 50%-90% wysokości
- D (dolny poziomy): 20%-80% szerokości, 83%-95% wysokości
- E (lewy dolny): 10%-22% szerokości, 50%-90% wysokości
- F (lewy górny): 10%-22% szerokości, 10%-50% wysokości
- G (środkowy poziomy): 20%-80% szerokości, 45%-57% wysokości

---

### `src/cli_parser.cpp`

**Opis:** Implementacja parsowania argumentów wiersza poleceń.

**Obsługiwane flagi:**
| Flaga | Opis |
|-------|------|
| `-m`, `--mode` | Tryb pracy (send/receive/display) |
| `-p`, `--port` | Ścieżka urządzenia UART |
| `-b`, `--baud` | Baudrate |
| `-i`, `--input` | Plik wideo |
| `-th`, `--threshold` | Próg binaryzacji |
| `-fps`, `--fpscap` | Limit FPS |
| `-res`, `--resolution` | Rozdzielczość W,H |
| `-div`, `--dividents` | Liczba segmentów X,Y |
| `-c`, `--color` | Kolor R,G,B |
| `-d`, `--draw` | Wyświetl okno |
| `-a`, `--audio` | Odtwarzaj audio |
| `-inv`, `--invert` | Inwertuj obraz |
| `-h`, `--help` | Pomoc |

---

### `src/uart.cpp`

**Opis:** Implementacja komunikacji szeregowej przez POSIX termios.

**Detale:**
- Używa `open()` z flagami O_RDWR | O_NOCTty
- Konfiguruje: 8N1 (8 bitów, bez parzystości, 1 bit stopu), raw mode
- Obsługuje baudrate: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
- `write()` używa `tcdrain()` do oczekiwania na wysłanie
- `read()` używa `select()` z timeoutem

---

### `src/audio.cpp`

**Opis:** Odtwarzanie audio z pliku wideo używając FFmpeg + SDL2.

**Detale:**
- Dekoduje strumień audio za pomocą FFmpeg
- Konwertuje format za pomocą swresample
- Odtwarza przez SDL2 AudioDevice
- Działa w osobnym wątku, kontrolowany przez atomic<bool> stop_flag

---

## Kompilacja i uruchomienie

```bash
cd uart_app
make          # kompilacja
./app --help  # wyświetl pomoc
```

### Przykłady użycia:

**Tryb send (wysyłanie przez UART):**
```bash
# Bez wyświetlania
./app -m send -p /dev/ttyUSB0 -b 115200 -i ../res/BadApple!!.mp4

# Z wyświetlaniem i audio
./app -m send -p /dev/ttyUSB0 -b 115200 -i ../res/BadApple!!.mp4 -d -a
```

**Tryb receive (odbieranie z UART):**
```bash
# Logowanie do konsoli (bez GUI)
sudo ./app -m receive -p /dev/ttyUSB0 -b 115200

# Z wyświetlaniem (wymaga X11)
sudo -E DISPLAY=:1 ./app -m receive -p /dev/ttyUSB0 -b 115200 -d

# Z wyświetlaniem na framebuffer (bez X11)
sudo QT_QPA_PLATFORM=linuxfb ./app -m receive -p /dev/ttyUSB0 -b 115200 -d
```

**Tryb display (lokalne wyświetlanie):**
```bash
./app -m display -d
```

---

## Protokół ramki

| Pole | Rozmiar | Opis |
|------|---------|------|
| start | 2 bajty | 0x2137 (little-endian: 0x37, 0x21) |
| max_X | 1 bajt | Liczba kolumn segmentów |
| max_Y | 1 bajt | Liczba wierszy segmentów |
| cells | max_X * max_Y bajtów | Stany segmentów (bitmaski 7-bitowe) |
| end | 1 bajt | 0x69 |

Całkowity rozmiar ramki: 5 + (max_X * max_Y) bajtów

---

## Uwagi

- Do uruchamiania z uprawnieniami UART może być potrzebne `sudo`
- Dla trybu receive z GUI, użyj `sudo -E DISPLAY=:1` aby zachować zmienne X11
- Bez flagi `-d` tryb receive działa bez GUI - loguje dane do konsoli