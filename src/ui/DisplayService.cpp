#include "DisplayService.h"
#include "PolishGlyphs.h"

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
// yoRadio yofont5x7.c glyphs 0x11 (prev), 0x0E (note), 0x10 (next).
static constexpr uint8_t NAV_PREV[] = {0x08, 0x1C, 0x3E, 0x7F, 0x00};
static constexpr uint8_t NAV_NOTE[] = {0x60, 0x7F, 0x05, 0x35, 0x3F};
static constexpr uint8_t NAV_NEXT[] = {0x00, 0x7F, 0x3E, 0x1C, 0x08};

static constexpr int LEFT_X = 2;

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

int DisplayService::glyphAdvance(uint32_t codepoint, bool artistFont, uint8_t scale) {
    if (codepoint >= 32 && codepoint <= 126) {
        if (artistFont) {
            return pgm_read_byte(&FreeSans9pt7bGlyphs[codepoint - 32].xAdvance);
        }
        return 6 * scale;
    }
    return 6 * (artistFont ? 2 : scale); // Local PL glyph or one visible fallback marker.
}

void DisplayService::drawUtf8Line(
    const String& value, int x, int y, int maxWidth,
    uint16_t color, uint16_t background, bool artistFont, uint8_t scale
) {
    _tft.setFont(artistFont ? &FreeSans9pt7b : nullptr);
    _tft.setTextSize(artistFont ? 1 : scale);
    _tft.setTextColor(color, background);

    const char* cursor = value.c_str();
    int fullWidth = 0;
    while (*cursor) {
        const uint32_t codepoint = PolishGlyphs::next(cursor);
        fullWidth += glyphAdvance(codepoint, artistFont, scale);
    }
    const bool clipped = fullWidth > maxWidth;
    const int textLimit = clipped ? maxWidth - glyphAdvance('.', artistFont, scale) * 3 : maxWidth;
    cursor = value.c_str();
    int drawnWidth = 0;
    while (*cursor) {
        const uint32_t codepoint = PolishGlyphs::next(cursor);
        const int advance = glyphAdvance(codepoint, artistFont, scale);
        if (drawnWidth + advance > textLimit) break;

        const PolishGlyphs::Glyph* glyph = PolishGlyphs::find(codepoint);
        if (glyph) {
            const int top = artistFont ? y - 15 : y;
            const uint8_t glyphScale = artistFont ? 2 : scale;
            for (uint8_t row = 0; row < 8; ++row) {
                for (uint8_t col = 0; col < 5; ++col) {
                    if (glyph->rows[row] & (1 << (4 - col))) {
                        _tft.fillRect(x + drawnWidth + col * glyphScale,
                                      top + row * glyphScale,
                                      glyphScale, glyphScale, color);
                    }
                }
            }
            _tft.setCursor(x + drawnWidth + advance, y);
        } else {
            // Unsupported codepoints remain intact in the source String.
            // A single '?' marks each glyph unavailable in these small fonts.
            _tft.setCursor(x + drawnWidth, y);
            _tft.write(codepoint >= 32 && codepoint <= 126
                ? static_cast<uint8_t>(codepoint) : static_cast<uint8_t>('?'));
        }
        drawnWidth += advance;
    }
    if (clipped) {
        _tft.setCursor(x + drawnWidth, y);
        _tft.print("...");
    }
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
        label = "Vox One";
    }

    drawUtf8Line(label, 3, 2, _tft.width() - 3,
                 kColorPrimaryText, stationFill, false, 2);
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

    drawUtf8Line(artist.isEmpty() ? String("-") : artist,
                 LEFT_X, ARTIST_BASELINE_Y, METADATA_W - LEFT_X,
                 kColorSecondaryText, kColorBackground, true, 1);
    drawUtf8Line(title.isEmpty() ? String("-") : title,
                 LEFT_X, TITLE2_Y, METADATA_W - LEFT_X,
                 kColorSonyBlue, kColorBackground, false, 1);
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
    // Center the complete visible indicator under the clock text, not merely
    // the background rectangle. Keep WIFI_Y and the bar heights unchanged.
    _tft.setFont();
    int16_t x1, y1;
    uint16_t clockWidth, textHeight;
    _tft.setTextSize(2);
    _tft.getTextBounds("--:--", 0, 0, &x1, &y1, &clockWidth, &textHeight);
    const int clockCenterX = CLOCK_X + static_cast<int>(clockWidth) / 2;

    _tft.setTextSize(1);
    const char* label = wifiConnected ? "WIFI" : (apMode ? "AP" : "WIFI --");
    uint16_t labelWidth;
    _tft.getTextBounds(label, 0, 0, &x1, &y1, &labelWidth, &textHeight);
    constexpr int barCount = 4;
    constexpr int barStride = 6;
    constexpr int barWidth = 4;
    constexpr int labelGap = 2;
    const int barsWidth = (barCount - 1) * barStride + barWidth;
    const int visibleWidth = static_cast<int>(labelWidth) +
        (wifiConnected ? labelGap + barsWidth : 0);
    const int labelX = clockCenterX - visibleWidth / 2;

    _tft.fillRect(CLOCK_X, WIFI_Y, CLOCK_W, 13, kColorBackground);
    _tft.setCursor(labelX, WIFI_Y);

    if (!wifiConnected) {
        _tft.setTextColor(
            apMode ? kColorSonyBlue : kColorInactive,
            kColorBackground
        );
        _tft.print(label);
        return;
    }

    _tft.setTextColor(kColorSecondaryText, kColorBackground);
    _tft.print(label);

    const int level = wifiLevel(wifiRssi);
    const int baseX = labelX + static_cast<int>(labelWidth) + labelGap;
    const int baseY = WIFI_Y + 10;

    for (int i = 0; i < barCount; ++i) {
        const int h = 2 + i * 2;
        const uint16_t color =
            i < level ? kColorSonyBlue : kColorInactive;

        _tft.fillRect(
            baseX + i * barStride,
            baseY - h,
            barWidth,
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
    _tft.setCursor(startX + iconWidth + 6, VOLUME_Y + 2);
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

void DisplayService::drawBtTrackNavScreen() {
    _tft.fillScreen(kColorBackground);
    _tft.fillRect(0, 0, _tft.width(), HEADER_H, kColorSurface);

    const String label(u8"BT - PRZEŁĄCZ UTWÓR");
    int labelWidth = 0;
    for (const char* cursor = label.c_str(); *cursor;)
        labelWidth += glyphAdvance(PolishGlyphs::next(cursor), false, 2);
    drawUtf8Line(label, (_tft.width() - labelWidth) / 2, 2, _tft.width(),
                 kColorPrimaryText, kColorSurface, false, 2);

    // Only the three small yoRadio symbols are retained, scaled for ST7789.
    constexpr int iconScale = 2;
    constexpr int iconWidth = 5 * iconScale;
    constexpr int iconGap = 40;
    constexpr int iconY = 37; // Keeps the symbols centered near their original y=44.
    const int leftX = (_tft.width() - (3 * iconWidth + 2 * iconGap)) / 2;
    auto drawIcon = [this, iconScale, iconY](const uint8_t* columns, int x,
                                             uint16_t color) {
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if (columns[column] & (1 << row))
                    _tft.fillRect(x + column * iconScale, iconY + row * iconScale,
                                  iconScale, iconScale, color);
            }
        }
    };
    drawIcon(NAV_PREV, leftX, kColorSonyBlue);
    drawIcon(NAV_NOTE, leftX + iconWidth + iconGap, kColorSecondaryText);
    drawIcon(NAV_NEXT, leftX + 2 * (iconWidth + iconGap), kColorSonyBlue);
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

void DisplayService::loop(UiMode mode) {
    if (_visualDisabled) return;
    // RadioList has no implementation; a caller cannot expose a blank screen.
    const UiMode visibleMode = mode == UiMode::RadioList ? UiMode::Home : mode;
    const DeviceState s = StateStore::instance().snapshot();

    if (!_layoutDrawn || visibleMode != _renderedMode) {
        _renderedMode = visibleMode;
        if (visibleMode == UiMode::Volume) {
            showVolumeScreen(s.volume, s.ip);
            _lastVolume = s.volume;
        } else if (visibleMode == UiMode::BtTrackNav) {
            drawBtTrackNavScreen();
        } else {
            drawStaticLayout();
            updatePlayerFields(true);
        }
        _layoutDrawn = true;
        return;
    }

    if (visibleMode == UiMode::Volume) {
        if (s.volume != _lastVolume) {
            _lastVolume = s.volume;
            // Keep the IP field untouched while the large number changes.
            drawVolumeValue(s.volume);
        }
        return;
    }
    if (visibleMode == UiMode::BtTrackNav) return;
    updatePlayerFields(false);
}
bool DisplayService::clearToBlack() {
    if (!_initialized) return false;
    _tft.fillScreen(kColorBlack);
    _layoutDrawn = false;
    _visualDisabled = true;
    return true;
}

void DisplayService::redraw() {
    if (_visualDisabled) return;
    _layoutDrawn = false;
    loop(_renderedMode);
}

