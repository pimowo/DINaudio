# ROADMAP

## M1 — FOUNDATION ✅
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

M2.2 is hardware-tested and accepted on firmware 0.2.1-m2.2.

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
- OTA rollback
