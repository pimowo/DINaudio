# TEST PLAN

## Zmiana na układ bez OTA — do weryfikacji sprzętowej

DINaudio nie obsługuje OTA; aktualizacje odbywają się przez USB/serial.
Wcześniejsze zaliczenia OTA poniżej są historyczne.

- [ ] pierwsze wgranie przewodowe bootloadera, nowej tabeli i aplikacji
- [ ] pojedynczy slot factory 0x3E0000 oraz zachowany coredump
- [ ] zachowanie konfiguracji NVS po standardowym uploadzie bez erase_flash
- [ ] WWW, Wi-Fi, mDNS i ekran działają
- [ ] Bluetooth A2DP/AVRCP, metadata, głośność i reconnect grace działają
- [ ] brak formularza OTA; GET/POST /update zwracają 404

## M1 — zaliczone

- [x] boot ESP32
- [x] TFT ST7789 284×76
- [x] enkoder obrót
- [x] enkoder click
- [x] poprawny kierunek enkodera
- [x] PCM5102A / I2S
- [x] ton 440 Hz
- [x] AP `DINaudio-XXXXXX`
- [x] `192.168.4.1`
- [x] zapis Wi-Fi
- [x] połączenie z LAN
- [x] WWW po LAN
- [x] OTA przez WWW
- [x] firmware po OTA uruchamia się poprawnie
- [x] `0.1.1-m1` potwierdzone po OTA

## M2.2 — zaliczone sprzętowo

- [x] Bluetooth init
- [x] A2DP audio przez PCM5102A
- [x] AVRCP metadata
- [x] AVRCP play/pause
- [x] AVRCP Next/Previous
- [x] dwukierunkowa synchronizacja głośności
- [x] connect/disconnect
- [x] BT + Wi-Fi
- [x] BT + WWW
- [x] OTA podczas pracy firmware
- [x] TFT BT status i metadata
- [x] brak migotania TFT
- [x] firmware 0.2.0 (milestone M2.2)

## 0.2.1 — fundament konfiguracji runtime

- [x] migracja istniejącej konfiguracji NVS przetestowana sprzętowo
- [x] schemaVersion: 1

Zmiany głośności z telefonu mogą przeskakiwać o kilka punktów z powodu
grubszej skali urządzenia źródłowego. Jest to zaakceptowane zachowanie.

## 0.4.0 — Bluetooth ownership i reconnect grace`r`n`r`n- [x] ownership BT przez App`r`n- [x] reconnect grace period 10 s i ekran BT RECONNECT`r`n- [x] reconnect ostatniego urządzenia oraz powrót bez końcowego STOP`r`n- [x] Config schema 3; migracja 1 → 2 → 3 zachowuje istniejącą konfigurację`r`n- [x] BT ownership/reconnect przetestowane sprzętowo`r`n`r`n## M2 — dalsza weryfikacja

- [ ] pairing management
- [ ] heap/min heap
- [ ] reconnect stress test

## Audio output ownership — pending hardware validation

- [x] `AudioOutputManager` jako wyłączny właściciel fizycznego I2S/PCM5102A
- [x] Bluetooth lifecycle `suspend/resume` przygotowany kodowo
- [x] czasowe zatrzymanie A2DP przez `end(false)`; `end(true)` nie jest używane
- [x] reconnect grace 10 s pozostaje bez zmian
- [ ] sprzętowy test runtime BT suspend/resume i ponownego przejęcia I2S
- [ ] sprzętowy test nowej tabeli partycji
