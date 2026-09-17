#include "DisplayService.h"

#include "BoardConfig.h"
#include "../core/StateStore.h"

DisplayService::DisplayService()
    : _tft(Board::TFT_CS, Board::TFT_DC, Board::TFT_RST) {}

void DisplayService::begin() {
    SPI.begin(Board::TFT_SCK, -1, Board::TFT_MOSI, Board::TFT_CS);

    _tft.init(Board::TFT_INIT_W, Board::TFT_INIT_H);
    _tft.setRotation(Board::TFT_ROTATION);
    _tft.invertDisplay(false);
    _tft.setTextWrap(false);

    drawStaticLayout();
    updateDynamicFields(true);
}

void DisplayService::drawStaticLayout() {
    _tft.fillScreen(ST77XX_BLACK);
    _tft.fillRect(0, 0, _tft.width(), 24, ST77XX_BLUE);

    _tft.setTextColor(ST77XX_WHITE);
    _tft.setTextSize(2);
    _tft.setCursor(8, 5);
    _tft.print("DINaudio M2.2");

    _layoutDrawn = true;
}

void DisplayService::drawVolume(int volume) {
    _tft.fillRect(8, 32, 120, 24, ST77XX_BLACK);

    _tft.setTextSize(2);
    _tft.setTextColor(ST77XX_WHITE);
    _tft.setCursor(8, 34);
    _tft.printf("VOL %3d", volume);
}

void DisplayService::drawBtState(bool connected, bool playing) {
    _tft.fillRect(140, 32, _tft.width() - 140, 24, ST77XX_BLACK);

    _tft.setTextSize(2);
    _tft.setCursor(145, 34);

    if (connected) {
        if (playing) {
            _tft.setTextColor(ST77XX_GREEN);
            _tft.print("BT PLAY");
        } else {
            _tft.setTextColor(ST77XX_CYAN);
            _tft.print("BT");
        }
    } else {
        _tft.setTextColor(ST77XX_YELLOW);
        _tft.print("STOP");
    }
}

void DisplayService::drawBottomLine(
    bool btConnected,
    const String& artist,
    const String& title,
    bool wifiConnected,
    const String& ip,
    bool apMode,
    const String& apSsid
) {
    _tft.fillRect(0, 58, _tft.width(), 18, ST77XX_BLACK);

    _tft.setTextSize(1);
    _tft.setTextColor(ST77XX_CYAN);
    _tft.setCursor(8, 62);

    if (btConnected && (!artist.isEmpty() || !title.isEmpty())) {
        String line;
        if (!artist.isEmpty()) line += artist;
        if (!artist.isEmpty() && !title.isEmpty()) line += " - ";
        if (!title.isEmpty()) line += title;

        // 284 px przy font 1 to ok. 46-47 znaków.
        if (line.length() > 45) {
            line = line.substring(0, 42) + "...";
        }
        _tft.print(line);
        return;
    }

    if (wifiConnected) {
        _tft.print(ip);
    } else if (apMode) {
        _tft.print("AP: ");
        _tft.print(apSsid);
    } else {
        _tft.print("WiFi: connecting...");
    }
}

void DisplayService::updateDynamicFields(bool force) {
    const DeviceState s = StateStore::instance().snapshot();

    if (force || s.volume != _lastVolume) {
        drawVolume(s.volume);
        _lastVolume = s.volume;
    }

    if (force ||
        s.bluetoothConnected != _lastBtConnected ||
        s.bluetoothPlaying != _lastBtPlaying) {

        drawBtState(s.bluetoothConnected, s.bluetoothPlaying);

        _lastBtConnected = s.bluetoothConnected;
        _lastBtPlaying = s.bluetoothPlaying;
    }

    if (force ||
        s.bluetoothConnected != _lastBtConnected ||
        s.bluetoothTitle != _lastBtTitle ||
        s.bluetoothArtist != _lastBtArtist ||
        s.wifiConnected != _lastWifiConnected ||
        s.ip != _lastIp ||
        s.apMode != _lastApMode ||
        s.apSsid != _lastApSsid) {

        drawBottomLine(
            s.bluetoothConnected,
            s.bluetoothArtist,
            s.bluetoothTitle,
            s.wifiConnected,
            s.ip,
            s.apMode,
            s.apSsid
        );

        _lastBtTitle = s.bluetoothTitle;
        _lastBtArtist = s.bluetoothArtist;
        _lastWifiConnected = s.wifiConnected;
        _lastIp = s.ip;
        _lastApMode = s.apMode;
        _lastApSsid = s.apSsid;
    }
}

void DisplayService::loop() {
    if (!_layoutDrawn) {
        drawStaticLayout();
        updateDynamicFields(true);
        return;
    }

    updateDynamicFields(false);
}

void DisplayService::redraw() {
    if (!_layoutDrawn) drawStaticLayout();
    updateDynamicFields(true);
}
