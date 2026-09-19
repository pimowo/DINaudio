#include "DisplayService.h"

#include "AppConfig.h"
#include "BoardConfig.h"
#include "../core/StateStore.h"
#include <Fonts/FreeSans9pt7b.h>

namespace {
constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(
        ((red & 0xF8) << 8) |
        ((green & 0xFC) << 3) |
        (blue >> 3)
    );
}

static constexpr uint16_t kColorBlack = rgb565(0, 0, 0);
static constexpr uint16_t kColorBackground = rgb565(6, 8, 12);
static constexpr uint16_t kColorSurface = rgb565(18, 22, 28);
static constexpr uint16_t kColorPrimaryText = rgb565(245, 245, 245);
static constexpr uint16_t kColorSonyBlue = rgb565(120, 170, 255);
static constexpr uint16_t kColorSonyBlueDark = rgb565(90, 140, 230);
static constexpr uint16_t kColorSecondaryText = rgb565(180, 200, 220);
static constexpr uint16_t kColorInfo = rgb565(80, 160, 220);
static constexpr uint16_t kColorNeutral = rgb565(200, 200, 200);
static constexpr uint16_t kColorDivider = rgb565(40, 90, 160);
static constexpr uint16_t kColorInactive = rgb565(40, 50, 65);
static constexpr uint16_t kColorSoftBlue = rgb565(180, 200, 255);
static constexpr int DSP_WIDTH = 284;
static constexpr int HEADER_H = 19;
static constexpr int ARTIST_BASELINE_Y = 35;
static constexpr int TITLE2_Y = 40;
static constexpr int INFO1_Y = 60;
static constexpr int METADATA_W = 174;
static constexpr int STATUS_W = 112;

static constexpr int WIFI_X = 234;
static constexpr int WIFI_Y = 63;
static constexpr uint32_t WIFI_DRAW_INTERVAL_MS = 15000;

static constexpr int CLOCK_X = 218;
static constexpr int CLOCK_Y = 42;
static constexpr int CLOCK_W = 64;
static constexpr int CLOCK_H = 18;

static constexpr int VOLUME_Y = 60;
static constexpr int SPEAKER_SCALE = 2;
// yoRadio yofont5x7.c, glyph 0x13 (speaker), five 7-bit columns.
static constexpr uint8_t SPEAKER_GLYPH[] = {0x00, 0x00, 0x18, 0x3C, 0x7E};

static constexpr int LEFT_X = 2;

static constexpr size_t HEADER_CHARS = 44;
static constexpr size_t TITLE_CHARS = 26;
}

DisplayService::DisplayService(const St7789Pins& pins)
    : _pins(pins), _tft(pins.cs, pins.dc, pins.rst) {}

void DisplayService::begin() {
    SPI.begin(
        _pins.sck,
        -1,
        _pins.mosi,
        _pins.cs
    );

    _tft.init(Board::TFT_INIT_W, Board::TFT_INIT_H);
    _tft.setRotation(Board::TFT_ROTATION);
    _tft.invertDisplay(false);
    _tft.setTextWrap(false);
    _initialized = true;

    drawStaticLayout();
    updatePlayerFields(true);
}

void DisplayService::drawStaticLayout() {
    _tft.fillScreen(kColorBackground);

    const uint16_t stationFill =
        kColorSurface;

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
        kColorSurface;

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
        label = "VoxOne";
    }

    label = fitText(
        tftText(label),
        HEADER_CHARS
    );

    _tft.setTextColor(kColorPrimaryText, stationFill);
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
    _tft.fillRect(0, HEADER_H + 1, METADATA_W, 29, kColorBackground);

    _tft.setFont();
    _tft.setTextSize(1);

    if (!btConnected && !reconnectGrace) {
        _tft.setTextColor(kColorInactive, kColorBackground);
        _tft.setCursor(LEFT_X, HEADER_H + 2);
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

    _tft.setFont(&FreeSans9pt7b);
    String artistText = artistLine.isEmpty() ? String("-") : artistLine;
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(artistText, 0, ARTIST_BASELINE_Y, &x1, &y1, &w, &h);
    while (artistText.length() > 1 && static_cast<int>(w) > METADATA_W - LEFT_X) {
        artistText.remove(artistText.length() - 1);
        _tft.getTextBounds(artistText, 0, ARTIST_BASELINE_Y, &x1, &y1, &w, &h);
    }
    _tft.setTextColor(kColorSecondaryText);
    _tft.setCursor(LEFT_X, ARTIST_BASELINE_Y);
    _tft.print(artistText);

    _tft.setFont();
    _tft.setTextColor(kColorSonyBlue, kColorBackground);
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
        STATUS_W,
        10,
        kColorBackground
    );

    _tft.setFont();
    _tft.setTextSize(1);
    _tft.setCursor(LEFT_X, INFO1_Y);

    if (reconnectGrace) {
        _tft.setTextColor(kColorSonyBlueDark, kColorBackground);
        _tft.print("BT A2DP  RECONNECT");
    } else if (btConnected) {
        _tft.setTextColor(kColorInactive, kColorBackground);
        _tft.print("BT A2DP");

        _tft.setTextColor(
            btPlaying ? kColorPrimaryText : kColorSonyBlueDark,
            kColorBackground
        );

        _tft.print(btPlaying ? "  PLAY" : "  PAUSE");
    } else {
        _tft.setTextColor(kColorSonyBlue, kColorBackground);
        _tft.print("STOP");
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
        50,
        13,
        kColorBackground
    );

    _tft.setTextSize(1);
    _tft.setCursor(WIFI_X, WIFI_Y);

    if (!wifiConnected) {
        _tft.setTextColor(
            apMode ? kColorSonyBlue : kColorInactive,
            kColorBackground
        );
        _tft.print(apMode ? "AP" : "WIFI --");
        return;
    }

    _tft.setTextColor(kColorSecondaryText, kColorBackground);
    _tft.print("WIFI");

    const int level = wifiLevel(wifiRssi);
    const int baseX = WIFI_X + 26;
    const int baseY = WIFI_Y + 10;

    for (int i = 0; i < 4; ++i) {
        const int h = 2 + i * 2;
        const uint16_t color =
            i < level ? kColorSonyBlue : kColorInactive;

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
        kColorBackground
    );

    _tft.setTextSize(2);
    _tft.setCursor(CLOCK_X, CLOCK_Y);

    if (valid && !clockText.isEmpty()) {
        _tft.setTextColor(kColorPrimaryText, kColorBackground);
        _tft.print(clockText);
    } else {
        _tft.setTextColor(kColorNeutral, kColorBackground);
        _tft.print("--:--");
    }
}

void DisplayService::drawVolumeIndicator(int volume) {
    _tft.fillRect(116, 55, 52, 19, kColorBackground);
    _tft.setFont();
    _tft.setTextSize(1);

    char value[4];
    snprintf(value, sizeof(value), "%d", constrain(volume, 0, 100));
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(value, 0, VOLUME_Y, &x1, &y1, &w, &h);

    const int iconWidth = 5 * SPEAKER_SCALE;
    const int startX = (DSP_WIDTH - (iconWidth + 6 + static_cast<int>(w))) / 2;
    for (int column = 0; column < 5; ++column) {
        for (int row = 0; row < 7; ++row) {
            if (SPEAKER_GLYPH[column] & (1 << row)) {
                _tft.fillRect(
                    startX + column * SPEAKER_SCALE,
                    57 + row * SPEAKER_SCALE,
                    SPEAKER_SCALE,
                    SPEAKER_SCALE,
                    kColorSonyBlue
                );
            }
        }
    }

    _tft.setTextColor(kColorPrimaryText, kColorBackground);
    _tft.setCursor(startX + iconWidth + 6, VOLUME_Y);
    _tft.print(value);
}

void DisplayService::drawVolumeValue(int volume) {
    _tft.fillRect(
        0,
        22,
        _tft.width(),
        36,
        kColorBackground
    );

    char vol[8];
    snprintf(
        vol,
        sizeof(vol),
        "%d",
        constrain(volume, 0, 100)
    );

    _tft.setTextColor(kColorSonyBlue, kColorBackground);
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
        kColorBackground
    );

    _tft.setTextSize(1);
    _tft.setTextColor(kColorSecondaryText, kColorBackground);
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

    _tft.fillScreen(kColorBackground);

    const uint16_t stationFill =
        kColorSurface;

    _tft.fillRect(
        0,
        0,
        _tft.width(),
        HEADER_H,
        stationFill
    );

    const char* label = "GLOSNOSC";

    _tft.setTextColor(kColorPrimaryText, stationFill);
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
        drawVolumeIndicator(s.volume);
        _lastVolume = s.volume;
    }

    _lastBtConnected = s.bluetoothConnected;
    _lastBtReconnectGrace = s.bluetoothReconnectGrace;
}

void DisplayService::loop() {
    if (_visualDisabled) return;

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

bool DisplayService::clearToBlack() {
    if (!_initialized) return false;
    _tft.fillScreen(kColorBlack);
    _layoutDrawn = false;
    _visualDisabled = true;
    _volumeScreenActive = false;
    return true;
}

void DisplayService::redraw() {
    if (_visualDisabled) return;
    _volumeScreenActive = false;
    drawStaticLayout();
    updatePlayerFields(true);
}

