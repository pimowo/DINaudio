# STATE MACHINE

Docelowe źródła:
- STOP
- RADIO
- BLUETOOTH
- PLAY_MEDIA

## RADIO -> BLUETOOTH
Po połączeniu telefonu radio zostaje zatrzymane, BT przejmuje urządzenie.

## BLUETOOTH -> RADIO / STOP
Po rozłączeniu 2–3 s na szybki reconnect.
Potem:
- wcześniejsze RADIO -> RADIO,
- wcześniejszy STOP -> STOP,
- radio wcześniej FAILED -> STOP.

## ANY -> PLAY_MEDIA
Zapamiętaj poprzednie źródło, odtwórz media i po zakończeniu przywróć poprzedni stan.

## M1
M1 wykorzystuje tylko STOP / PLAY testowego tonu jako test fundamentu Core.
