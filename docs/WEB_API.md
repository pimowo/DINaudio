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

API VoxOne pozostaje wersjonowane jako `/api/v1/...`.
VoxOne nie obsługuje OTA; GET/POST `/update` nie są zarejestrowane i zwracają 404.
Firmware aktualizuje się przez USB/serial.
