# WEB API

## Aktualne endpointy

- `GET /`
- `GET /assets/voxone.css`
- `GET /api/v1/status`
- `GET /api/v1/config`
- `POST /api/v1/config`
- `POST /api/v1/config/reset`
- `POST /wifi/save`
- `POST /wifi/clear`
- `POST /play`
- `POST /pause`
- `POST /next`
- `POST /previous`
- `POST /stop`
- `POST /volume`
- `POST /reboot`

API VoxOne pozostaje wersjonowane jako `/api/v1/...`.
VoxOne nie obsługuje OTA; GET/POST `/update` nie są zarejestrowane i zwracają 404.
Firmware aktualizuje się przez USB/serial.
Pole `ui.navigationTimeoutMs` (dokumentacyjnie `ui.navigation_timeout_ms`)
ustawia po restarcie timeout BT NAV i przyszłej listy stacji: 1000–30000 ms,
domyślnie 5000 ms. Ekran głośności nadal ma timeout 2500 ms.
Formularz konfiguracji przesyła pełny `RuntimeConfig` jako
`application/x-www-form-urlencoded`. Odpowiedź GET nie zawiera haseł Wi-Fi
ani MQTT; puste pole hasła w POST zachowuje starą wartość. Walidacja błędnego
formularza zwraca HTTP 400, błąd NVS HTTP 500. Udany zapis pełnego snapshotu
odpowiada HTTP 200 i planuje restart po około 1000 ms, bez hot-reloadu.
Reset wymaga `confirm=RESET`; czyszczenie Wi-Fi i reboot wymagają
`confirm=YES`. Wszystkie mutujące endpointy wymagają pola `_token`
zwracanego przez GET konfiguracji. Token ogranicza CSRF, ale nie jest
uwierzytelnianiem użytkownika. Endpointy sterowania odtwarzaniem i głośnością
pozostają operacjami live.
