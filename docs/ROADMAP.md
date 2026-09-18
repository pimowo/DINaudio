# ROADMAP

## Aktualizacje — decyzja projektowa

DINaudio nie obsługuje OTA ani rollbacku OTA. Firmware aktualizuje się przez USB/serial.
Jeden duży slot aplikacji factory: 0x3E0000 = 4 063 232 B.
Zmiana tabeli partycji wymaga pierwszego wgrania przewodowego.
Wzmianki o OTA w zakończonych milestone'ach są historyczne.

## 0.3.0 — display i NTP ✅

ST7789 284×76 oraz zegar NTP Europe/Warsaw zostały przetestowane sprzętowo.

## 0.4.0 — Bluetooth ownership i reconnect grace ✓`r`n- App zarządza ownership; grace period 10 s i reconnect ostatniego urządzenia.`r`n- Config schema 3 z migracją zachowującą istniejącą konfigurację.`r`n- Przetestowane sprzętowo.`r`n`r`n## M1 — FOUNDATION ✅
- BoardConfig
- StateStore
- CommandQueue
- TFT
- enkoder
- PCM5102A
- Wi-Fi
- AP setup
- WWW
- mDNS
- OTA

## M2 — BLUETOOTH

### M2.1 — A2DP baseline ✅
- A2DP Sink
- PCM5102A audio
- coexistence BT + Wi-Fi
- WWW and OTA

### M2.2 — AVRCP and metadata ✅
- AVRCP Play/Pause/Next/Previous
- metadata artist/title
- peer/device info when available
- bidirectional volume sync
- TFT BT status and metadata

M2.2 is hardware-tested and accepted on firmware 0.2.0.

### 0.2.1 — runtime configuration foundation

Wewnętrzny fundament konfiguracji runtime: ConfigModel, ConfigManager,
schemaVersion 1 oraz sprzętowo przetestowana migracja istniejącej konfiguracji NVS.

### Remaining M2 verification
- pairing management
- reconnect stress test

## M3 — RADIO
- HTTP/HTTPS stream
- playlist
- retry/reconnect
- codecs
- Next/Prev

## M4 — SOURCE MANAGER
- RADIO ↔ BT
- PLAY_MEDIA
- restore previous source
- technical mute
- fade transitions

## M5 — PROTOCOLS
- yoRadio `/ws`
- MQTT
- HA Discovery

## M6 — FINALIZATION
- final WWW
- storage migrations
- backup/restore
- safe mode
- diagnostics
- przewodowe recovery firmware przez USB/serial
