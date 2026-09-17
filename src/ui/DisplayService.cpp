#include "DisplayService.h"
#include "BoardConfig.h"
#include "AppConfig.h"
#include "../core/StateStore.h"

DisplayService::DisplayService()
    : _tft(Board::TFT_CS, Board::TFT_DC, Board::TFT_RST) {}

void DisplayService::begin() {
    SPI.begin(Board::TFT_SCK, -1, Board::TFT_MOSI, Board::TFT_CS);
    _tft.init(Board::TFT_INIT_W, Board::TFT_INIT_H);
    _tft.setRotation(Board::TFT_ROTATION);
    _tft.invertDisplay(false);
    _tft.setTextWrap(false);
    _tft.fillScreen(ST77XX_BLACK);
    redraw();
}

void DisplayService::loop() {
    if (millis() - _lastRefresh >= 1000) {
        _lastRefresh = millis();
        redraw();
    }
}

void DisplayService::redraw() {
    DeviceState s = StateStore::instance().snapshot();

    _tft.fillScreen(ST77XX_BLACK);

    _tft.fillRect(0, 0, _tft.width(), 24, ST77XX_BLUE);
    _tft.setTextColor(ST77XX_WHITE);
    _tft.setTextSize(2);
    _tft.setCursor(8, 5);
    _tft.print("DINaudio M1");

    _tft.setTextSize(2);
    _tft.setTextColor(ST77XX_WHITE);
    _tft.setCursor(8, 34);
    _tft.printf("VOL %3d", s.volume);

    _tft.setCursor(145, 34);
    if (s.playback == PlaybackState::Playing) {
        _tft.setTextColor(ST77XX_GREEN);
        _tft.print("PLAY");
    } else {
        _tft.setTextColor(ST77XX_YELLOW);
        _tft.print("STOP");
    }

    _tft.setTextSize(1);
    _tft.setTextColor(ST77XX_CYAN);
    _tft.setCursor(8, 62);

    if (s.wifiConnected) {
        _tft.print(s.ip);
    } else if (s.apMode) {
        _tft.print("AP: ");
        _tft.print(s.apSsid);
    } else {
        _tft.print("WiFi: connecting...");
    }
}
