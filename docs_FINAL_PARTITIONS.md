# DINaudio — finalny układ partycji 4 MB

Cel: wykonać jeden planowany flash kablowy i później pozostać przy OTA.

## Układ

- NVS: 20 KiB
- OTA data: 8 KiB
- app0: 0x1F0000 = 2 031 616 B
- app1: 0x1F0000 = 2 031 616 B
- coredump: 64 KiB

Granice:
- app0: 0x010000 .. 0x1FFFFF
- app1: 0x200000 .. 0x3EFFFF
- coredump: 0x3F0000 .. 0x3FFFFF

## Dlaczego bez filesystemu

Przy 4 MB flash priorytetem jest zachowanie dwóch możliwie największych
slotów OTA. Osobny LittleFS/SPIFFS zabrałby miejsce potrzebne przyszłemu
firmware z Bluetooth i radiem.

Konfigurację i playlistę projektujemy później oszczędnie. Jeśli rozmiar
playlisty wymusi trwały filesystem, będzie to wymagało świadomego kompromisu
albo sprzętu z większym flash — nie zmieniamy partycji pochopnie.

## Ważna granica

Każdy obraz firmware przeznaczony do OTA musi mieć mniej niż 2 031 616 B.

Zalecenie projektowe:
- ostrzeżenie przy ~1.90 MB,
- twardy stop przed limitem slotu,
- każdą dużą bibliotekę oceniać pod kątem Flash.

## Procedura

1. Skopiuj `platformio.ini` i `partitions.csv` do katalogu głównego DINaudio.
2. Wykonaj BUILD.
3. Jeśli build jest SUCCESS i program mieści się w app0/app1:
   wykonaj jednorazowo `Upload` po USB.
4. Ten upload zapisze także nową tablicę partycji.
5. Po poprawnym uruchomieniu sprawdź:
   - TFT,
   - Wi-Fi,
   - WWW,
   - Bluetooth,
   - audio.
6. Następną aktualizację wykonujemy już przez WWW OTA.

## Uwaga

Nie używaj już wcześniejszego patcha z partycjami `1900K`.
Ten plik jest docelowym układem dla obecnego ESP32 4 MB.
