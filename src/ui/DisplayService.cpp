#include "DisplayService.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "../core/StateStore.h"

namespace {
static constexpr uint16_t COLOR_DARK_GREY = 0x4208;

static constexpr int DSP_WIDTH = 284;
static constexpr int HEADER_H = 19;
static constexpr int TITLE1_Y = 21;
static constexpr int TITLE2_Y = 30;
static constexpr int INFO1_Y = 43;

static constexpr int WIFI_X = 2;
static constexpr int WIFI_Y = 55;
static constexpr uint32_t WIFI_DRAW_INTERVAL_MS = 15000;

static constexpr int CLOCK_X = 218;
static constexpr int CLOCK_Y = 45;
static constexpr int CLOCK_W = 64;
static constexpr int CLOCK_H = 18;

static constexpr int VOLBAR_X = 2;
static constexpr int VOLBAR_Y = 72;
static constexpr int VOLBAR_W = 280;
static constexpr int VOLBAR_H = 3;

static constexpr int LEFT_X = 2;

static constexpr size_t HEADER_CHARS = 44;
static constexpr size_t TITLE_CHARS = 26;
}

DisplayService::DisplayService()
    : _tft(Board::TFT_CS, Board::TFT_DC, Board::TFT_RST) {}

void DisplayService::begin() {
    SPI.begin(
        Board::TFT_SCK,
        -1,
        Board::TFT_MOSI,
        Board::TFT_CS
    );

    _tft.init(Board::TFT_INIT_W, Board::TFT_INIT_H);
    _tft.setRotation(Board::TFT_ROTATION);
    _tft.invertDisplay(false);
    _tft.setTextWrap(false);

    drawStaticLayout();
    updatePlayerFields(true);
}

void DisplayService::drawStaticLayout() {
    _tft.fillScreen(ST77XX_BLACK);

    const uint16_t stationFill =
        _tft.color565(231, 211, 90);

    _tft.fillRect(
        0,
        0,
        _tft.width(),
        HEADER_H,
        stationFill
    );

    _tft.drawFastHLine(
        0,
        HEADER_H,
        _tft.width(),
        stationFill
    );

    _tft.drawRect(
        VOLBAR_X,
        VOLBAR_Y,
        VOLBAR_W,
        VOLBAR_H + 1,
        COLOR_DARK_GREY
    );

    _layoutDrawn = true;
}

String DisplayService::tftText(
    const String& value,
    bool uppercase
) {
    String out;
    out.reserve(value.length());

    const uint8_t* p =
        reinterpret_cast<const uint8_t*>(value.c_str());

    while (*p) {
        if (p[0] == 0xC4 && p[1]) {
            switch (p[1]) {
                case 0x84: case 0x85: out += 'A'; p += 2; continue;
                case 0x86: case 0x87: out += 'C'; p += 2; continue;
                case 0x98: case 0x99: out += 'E'; p += 2; continue;
                default: break;
            }
        }

        if (p[0] == 0xC5 && p[1]) {
            switch (p[1]) {
                case 0x81: case 0x82: out += 'L'; p += 2; continue;
                case 0x83: case 0x84: out += 'N'; p += 2; continue;
                case 0x9A: case 0x9B: out += 'S'; p += 2; continue;
                case 0xB9: case 0xBA: out += 'Z'; p += 2; continue;
                case 0xBB: case 0xBC: out += 'Z'; p += 2; continue;
                default: break;
            }
        }

        if (p[0] == 0xC3 && p[1]) {
            switch (p[1]) {
                case 0x93: case 0xB3: out += 'O'; p += 2; continue;
                default: break;
            }
        }

        if (*p >= 32 && *p <= 126) {
            out += static_cast<char>(*p);
        } else {
            out += '?';
        }

        ++p;
    }

    if (uppercase) {
        out.toUpperCase();
    }

    return out;
}

String DisplayService::fitText(
    const String& value,
    size_t maxChars
) {
    if (value.length() <= maxChars) {
        return value;
    }

    if (maxChars <= 3) {
        return value.substring(0, maxChars);
    }

    return value.substring(0, maxChars - 3) + "...";
}

int DisplayService::wifiLevel(int rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

void DisplayService::drawHeader(
    bool btConnected,
    bool reconnectGrace,
    const String& peerName
) {
    const uint16_t stationFill =
        _tft.color565(231, 211, 90);

    _tft.fillRect(
        0,
        0,
        _tft.width(),
        HEADER_H,
        stationFill
    );

    String label;

    if (btConnected || reconnectGrace) {
        label = peerName.isEmpty()
            ? String("BLUETOOTH")
            : peerName;
    } else {
        label = "DINaudio";
    }

    label = fitText(
        tftText(label),
        HEADER_CHARS
    );

    _tft.setTextColor(ST77XX_BLACK, stationFill);
    _tft.setTextSize(2);
    _tft.setCursor(3, 2);
    _tft.print(label);
}

void DisplayService::drawMetadata(
    bool btConnected,
    bool reconnectGrace,
    const String& artist,
    const String& title
) {
    _tft.fillRect(
        0,
        TITLE1_Y,
        174,
        20,
        ST77XX_BLACK
    );

    _tft.setTextSize(1);

    if (!btConnected && !reconnectGrace) {
        _tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        _tft.setCursor(LEFT_X, TITLE1_Y);
        _tft.print("STOP");
        return;
    }

    String artistLine = fitText(
        tftText(artist),
        TITLE_CHARS
    );

    String titleLine = fitText(
        tftText(title),
        TITLE_CHARS
    );

    _tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    _tft.setCursor(LEFT_X, TITLE1_Y);
    _tft.print(artistLine.isEmpty() ? "-" : artistLine);

    _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    _tft.setCursor(LEFT_X, TITLE2_Y);
    _tft.print(titleLine.isEmpty() ? "-" : titleLine);
}

void DisplayService::drawSourceInfo(
    bool btConnected,
    bool btPlaying,
    bool reconnectGrace
) {
    _tft.fillRect(
        0,
        INFO1_Y,
        174,
        10,
        ST77XX_BLACK
    );

    _tft.setTextSize(1);
    _tft.setCursor(LEFT_X, INFO1_Y);

    if (reconnectGrace) {
        _tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        _tft.print("BT A2DP  RECONNECT");
    } else if (btConnected) {
        _tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
        _tft.print("BT A2DP");

        _tft.setTextColor(
            btPlaying ? ST77XX_GREEN : ST77XX_YELLOW,
            ST77XX_BLACK
        );

        _tft.print(btPlaying ? "  PLAY" : "  PAUSE");
    } else {
        _tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        _tft.print("SOURCE: STOP");
    }
}

void DisplayService::drawWifiIndicator(
    bool wifiConnected,
    int wifiRssi,
    bool apMode
) {
    _tft.fillRect(
        WIFI_X,
        WIFI_Y,
        78,
        14,
        ST77XX_BLACK
    );

    _tft.setTextSize(1);
    _tft.setCursor(WIFI_X, WIFI_Y);

    if (!wifiConnected) {
        _tft.setTextColor(
            apMode ? ST77XX_YELLOW : COLOR_DARK_GREY,
            ST77XX_BLACK
        );
        _tft.print(apMode ? "AP" : "WIFI --");
        return;
    }

    _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    _tft.print("WIFI");

    const int level = wifiLevel(wifiRssi);
    const int baseX = 31;
    const int baseY = WIFI_Y + 9;

    for (int i = 0; i < 4; ++i) {
        const int h = 2 + i * 2;
        const uint16_t color =
            i < level ? ST77XX_GREEN : COLOR_DARK_GREY;

        _tft.fillRect(
            baseX + i * 6,
            baseY - h,
            4,
            h,
            color
        );
    }
}

void DisplayService::drawClock(
    bool valid,
    const String& clockText
) {
    _tft.fillRect(
        CLOCK_X,
        CLOCK_Y,
        CLOCK_W,
        CLOCK_H,
        ST77XX_BLACK
    );

    _tft.setTextSize(2);
    _tft.setCursor(CLOCK_X, CLOCK_Y);

    if (valid && !clockText.isEmpty()) {
        _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        _tft.print(clockText);
    } else {
        _tft.setTextColor(COLOR_DARK_GREY, ST77XX_BLACK);
        _tft.print("--:--");
    }
}

void DisplayService::drawVolumeBar(int volume) {
    const int safeVolume = constrain(volume, 0, 100);
    const int innerW = VOLBAR_W - 2;
    const int filled =
        map(safeVolume, 0, 100, 0, innerW);

    _tft.fillRect(
        VOLBAR_X + 1,
        VOLBAR_Y + 1,
        innerW,
        VOLBAR_H - 1,
        ST77XX_BLACK
    );

    if (filled > 0) {
        _tft.fillRect(
            VOLBAR_X + 1,
            VOLBAR_Y + 1,
            filled,
            VOLBAR_H - 1,
            ST77XX_CYAN
        );
    }

    _tft.drawRect(
        VOLBAR_X,
        VOLBAR_Y,
        VOLBAR_W,
        VOLBAR_H + 1,
        COLOR_DARK_GREY
    );
}

void DisplayService::drawVolumeValue(int volume) {
    _tft.fillRect(
        0,
        22,
        _tft.width(),
        36,
        ST77XX_BLACK
    );

    char vol[8];
    snprintf(
        vol,
        sizeof(vol),
        "%d",
        constrain(volume, 0, 100)
    );

    _tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    _tft.setTextSize(3);

    int16_t x1, y1;
    uint16_t w, h;

    _tft.getTextBounds(
        vol,
        0,
        0,
        &x1,
        &y1,
        &w,
        &h
    );

    _tft.setCursor(
        (_tft.width() - static_cast<int>(w)) / 2,
        27
    );

    _tft.print(vol);
}

void DisplayService::drawVolumeIp(const String& ip) {
    _tft.fillRect(
        0,
        60,
        _tft.width(),
        12,
        ST77XX_BLACK
    );

    _tft.setTextSize(1);
    _tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    _tft.setCursor(3, 62);

    if (ip.isEmpty()) {
        _tft.print("IP: -");
    } else {
        _tft.print("IP: ");
        _tft.print(ip);
    }
}

void DisplayService::showVolumeScreen(
    int volume,
    const String& ip
) {
    _volumeScreenActive = true;
    _volumeScreenUntil =
        millis() + AppConfig::VOLUME_SCREEN_TIMEOUT_MS;

    _tft.fillScreen(ST77XX_BLACK);

    const uint16_t stationFill =
        _tft.color565(231, 211, 90);

    _tft.fillRect(
        0,
        0,
        _tft.width(),
        HEADER_H,
        stationFill
    );

    const char* label = "GLOSNOSC";

    _tft.setTextColor(ST77XX_BLACK, stationFill);
    _tft.setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;

    _tft.getTextBounds(
        label,
        0,
        0,
        &x1,
        &y1,
        &w,
        &h
    );

    _tft.setCursor(
        (_tft.width() - static_cast<int>(w)) / 2,
        2
    );

    _tft.print(label);

    drawVolumeValue(volume);
    drawVolumeIp(ip);
}

void DisplayService::updatePlayerFields(bool force) {
    const DeviceState s =
        StateStore::instance().snapshot();

    if (force ||
        s.bluetoothConnected != _lastBtConnected ||
        s.bluetoothReconnectGrace != _lastBtReconnectGrace ||
        s.bluetoothPeerName != _lastBtPeer) {

        drawHeader(
            s.bluetoothConnected,
            s.bluetoothReconnectGrace,
            s.bluetoothPeerName
        );

        _lastBtPeer = s.bluetoothPeerName;
    }

    if (force ||
        s.bluetoothConnected != _lastBtConnected ||
        s.bluetoothReconnectGrace != _lastBtReconnectGrace ||
        s.bluetoothArtist != _lastBtArtist ||
        s.bluetoothTitle != _lastBtTitle) {

        drawMetadata(
            s.bluetoothConnected,
            s.bluetoothReconnectGrace,
            s.bluetoothArtist,
            s.bluetoothTitle
        );

        _lastBtArtist = s.bluetoothArtist;
        _lastBtTitle = s.bluetoothTitle;
    }

    if (force ||
        s.bluetoothConnected != _lastBtConnected ||
        s.bluetoothPlaying != _lastBtPlaying ||
        s.bluetoothReconnectGrace != _lastBtReconnectGrace) {

        drawSourceInfo(
            s.bluetoothConnected,
            s.bluetoothPlaying,
            s.bluetoothReconnectGrace
        );

        _lastBtPlaying = s.bluetoothPlaying;
    }

    const int level =
        s.wifiConnected ? wifiLevel(s.wifiRssi) : -1;

    const bool wifiStateChanged =
        s.wifiConnected != _lastWifiConnected ||
        s.apMode != _lastApMode;

    const bool wifiLevelDue =
        level != _lastWifiLevel &&
        (millis() - _lastWifiDrawMs >= WIFI_DRAW_INTERVAL_MS);

    if (force ||
        wifiStateChanged ||
        wifiLevelDue) {

        drawWifiIndicator(
            s.wifiConnected,
            s.wifiRssi,
            s.apMode
        );

        _lastWifiConnected = s.wifiConnected;
        _lastApMode = s.apMode;
        _lastWifiLevel = level;
        _lastWifiDrawMs = millis();
    }

    if (force ||
        s.timeValid != _lastTimeValid ||
        s.clockText != _lastClockText) {

        drawClock(
            s.timeValid,
            s.clockText
        );

        _lastTimeValid = s.timeValid;
        _lastClockText = s.clockText;
    }

    if (force || s.volume != _lastVolume) {
        drawVolumeBar(s.volume);
        _lastVolume = s.volume;
    }

    _lastBtConnected = s.bluetoothConnected;
    _lastBtReconnectGrace = s.bluetoothReconnectGrace;
}

void DisplayService::loop() {
    if (!_layoutDrawn) {
        drawStaticLayout();
        updatePlayerFields(true);
        return;
    }

    const DeviceState s =
        StateStore::instance().snapshot();

    if (_volumeScreenActive) {
        if (s.volume != _lastVolume) {
            _lastVolume = s.volume;
            _volumeScreenUntil =
                millis() + AppConfig::VOLUME_SCREEN_TIMEOUT_MS;

            // Only redraw the large number.
            // IP remains untouched, so it does not blink.
            drawVolumeValue(s.volume);
        }

        if ((int32_t)(millis() - _volumeScreenUntil) >= 0) {
            _volumeScreenActive = false;
            drawStaticLayout();
            updatePlayerFields(true);
        }

        return;
    }

    if (_lastVolume >= 0 &&
        s.volume != _lastVolume) {

        _lastVolume = s.volume;
        showVolumeScreen(s.volume, s.ip);
        return;
    }

    updatePlayerFields(false);
}

void DisplayService::redraw() {
    _volumeScreenActive = false;
    drawStaticLayout();
    updatePlayerFields(true);
}

