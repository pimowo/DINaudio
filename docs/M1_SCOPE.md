# DINaudio M1 — zakres

## Cel

Stworzyć pierwszy stabilny fundament działający na klasycznym ESP32 z istniejącego radia i uzyskać możliwość dalszej pracy bez kabla USB.

## Zasady

- Core nie zna konkretnych GPIO.
- GPIO są wyłącznie w `BoardConfig`.
- Display tylko czyta stan.
- Enkoder tylko publikuje komendy.
- Audio jest oddzielną usługą.
- Wi‑Fi i WWW nie sterują audio bezpośrednio.
- Wszystkie wejścia trafiają do `CommandQueue`.
- `StateStore` jest źródłem prawdy dla UI.

## Następny milestone

M2:
- Bluetooth A2DP Sink + AVRCP
- globalna głośność DINaudio <-> AVRCP
- status telefonu na TFT
- test współistnienia Bluetooth + Wi‑Fi + WWW
- pomiary heap/min-heap
