# DINaudio — MASTER SPEC

Status: baza po zakończeniu M1  
Firmware bazowy: `0.1.1-m1`

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

## Następny milestone

M2 — Bluetooth A2DP + AVRCP.
