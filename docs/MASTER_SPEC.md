# DINaudio — MASTER SPEC

Status: baza po zakończeniu M2.2 i wdrożeniu fundamentu konfiguracji runtime
Aktualna wersja firmware: 0.3.0

Wersjonowanie firmware stosuje Semantic Versioning MAJOR.MINOR.PATCH: 0.x.y oznacza
okres przed stabilnym 1.0.0, PATCH oznacza poprawki błędów, MINOR nowe funkcje, a
MAJOR niekompatybilne zmiany architektury lub API. M1, M2.1 i M2.2 są historycznymi
nazwami milestone'ów.

## Cel

DINaudio to autonomiczny moduł audio na klasycznym ESP32:
- radio internetowe,
- Bluetooth A2DP,
- `play_media` / TTS z Home Assistant,
- WWW,
- MQTT / HA,
- kompatybilny WebSocket yoRadio,
- pełna praca także bez HA i MQTT.

## Potwierdzony sprzęt referencyjny

Profil: `yoradio-esp32u-st7789-76-pcm5102a`

- ESP32-D0WD-V3 rev. 3.1
- flash 4 MB
- TFT ST7789 284×76
- PCM5102A
- enkoder GPIO35 / GPIO33 / GPIO32
- `ENCODER_DIRECTION = -1`
- I2S DOUT 27 / BCLK 26 / WS 25
- TFT SCK 18 / MOSI 23 / CS 5 / DC 4 / RST -1
- TFT init 76×284, rotation 1

## Architektura

- `StateStore` — jedno źródło prawdy.
- `CommandQueue` — centralna kolejka komend.
- display tylko obserwuje stan.
- input tylko generuje komendy.
- GPIO tylko w `BoardConfig`.
- Core niezależny od konkretnego sprzętu.
- moduły pracują nieblokująco.

## Priorytety źródeł

1. PLAY_MEDIA / TTS
2. Bluetooth po połączeniu telefonu
3. Radio
4. STOP

## M1 zakończone

- TFT
- enkoder
- PCM5102A
- Wi-Fi
- AP konfiguracji
- WWW
- mDNS
- OTA przez WWW

## M2.2 zakończone sprzętowo

Wersja: 0.2.0

## 0.2.1 — fundament konfiguracji runtime

Wersja 0.2.1 wprowadza ConfigModel, ConfigManager oraz schemaVersion 1.
Zachowano istniejące klucze NVS, a migracja istniejącej konfiguracji została
przetestowana sprzętowo.

## 0.3.0 — display i zegar

Wersja 0.3.0 obejmuje przebudowę ekranu ST7789 284×76 oraz TimeService
z configTzTime() dla Europe/Warsaw. Ekran playera, ekran głośności i NTP
zostały przetestowane sprzętowo.

- Bluetooth A2DP przez PCM5102A
- AVRCP Play/Pause/Next/Previous
- dwukierunkowa synchronizacja głośności
- metadata artist/title oraz peer/device info, jeśli dostępne
- równoległa praca Bluetooth, Wi-Fi, WWW i OTA
- TFT z BT status/metadata bez migotania

## Następny milestone

M3 — Radio.
