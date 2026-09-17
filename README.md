# DINaudio

Autonomiczny moduł audio na klasycznym ESP32.

Aktualny stan: **M2.2 zakończone i zweryfikowane sprzętowo**.
Aktualna wersja firmware: **0.2.0**.

## Wersjonowanie

DINaudio używa Semantic Versioning MAJOR.MINOR.PATCH. Wersje 0.x.y oznaczają okres
przed stabilnym 1.0.0; PATCH oznacza poprawki błędów, MINOR nowe funkcje, a MAJOR
niekompatybilne zmiany architektury lub API. Nazwy M1, M2.1 i M2.2 pozostają nazwami
historycznych milestone'ów.

## Struktura

```text
DINaudio/
├── include/
│   ├── AppConfig.h
│   ├── BoardConfig.h
│   └── BuildInfo.h
├── src/
│   ├── audio/
│   ├── core/
│   ├── diagnostics/
│   ├── hal/
│   ├── network/
│   ├── storage/
│   ├── ui/
│   └── main.cpp
├── docs/
│   ├── MASTER_SPEC.md
│   ├── ARCHITECTURE.md
│   ├── HARDWARE_PROFILES.md
│   ├── STATE_MACHINE.md
│   ├── WEB_API.md
│   ├── WEB_UI.md
│   ├── STORAGE.md
│   ├── RECOVERY.md
│   ├── MQTT.md
│   ├── YORADIO_COMPAT.md
│   ├── TEST_PLAN.md
│   └── ROADMAP.md
├── CHANGELOG.md
└── platformio.ini
```

## M1 — gotowe
- sprzęt referencyjny
- TFT
- enkoder
- PCM5102A
- Wi-Fi
- AP setup
- WWW
- mDNS
- OTA przez WWW

## Następny etap
**M2 — Bluetooth A2DP + AVRCP**
