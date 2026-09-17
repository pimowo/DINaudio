# DINaudio

Autonomiczny moduł audio na klasycznym ESP32.

Aktualny etap: **M1 zakończony i zweryfikowany**.  
Firmware bazowy: **0.1.1-m1**.

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
