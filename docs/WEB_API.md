# WEB API

## Aktualne endpointy

- `GET /`
- `GET /api/v1/status`
- `POST /wifi/save`
- `POST /wifi/clear`
- `POST /play`
- `POST /pause`
- `POST /next`
- `POST /previous`
- `POST /stop`
- `POST /volume`
- `POST /reboot`

API DINaudio pozostaje wersjonowane jako `/api/v1/...`.
DINaudio nie obsługuje OTA; GET/POST `/update` nie są zarejestrowane i zwracają 404.
Firmware aktualizuje się przez USB/serial.
