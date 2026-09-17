# ARCHITECTURE

## Warstwy

- `hal/` — wejścia fizyczne i adaptery hardware
- `core/` — stan i komendy
- `audio/` — I2S i źródła audio
- `network/` — Wi-Fi, WWW, później MQTT/WS
- `storage/` — NVS, config, migracje
- `ui/` — display
- `diagnostics/` — logi i health

## Reguły

- GPIO wyłącznie w `BoardConfig`.
- UI nie steruje audio bezpośrednio.
- Enkoder nie zmienia stanu bezpośrednio.
- Wszystkie komendy przez `CommandQueue`.
- `StateStore` jest źródłem prawdy.
- Brak długich blokujących operacji w runtime.
