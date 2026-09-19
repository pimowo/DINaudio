#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "../config/ConfigModel.h"
#include "UiMode.h"

class DisplayService {
public:
    explicit DisplayService(const St7789Pins& pins);

    void begin();
    void loop(UiMode mode);
    void redraw();
    bool clearToBlack();
    bool isInitialized() const { return _initialized; }

private:
    St7789Pins _pins;
    Adafruit_ST7789 _tft;

    bool _initialized = false;
    bool _visualDisabled = false;
    bool _layoutDrawn = false;
    UiMode _renderedMode = UiMode::Home; // Presentation cache; App owns UiMode.

    int _lastVolume = -1;
    bool _lastBtConnected = false;
    bool _lastBtPlaying = false;
    bool _lastBtReconnectGrace = false;
    String _lastBtPeer;
    String _lastBtTitle;
    String _lastBtArtist;

    int _lastWifiLevel = -1;
    bool _lastWifiConnected = false;
    bool _lastApMode = false;
    uint32_t _lastWifiDrawMs = 0;

    bool _lastTimeValid = false;
    String _lastClockText;

    void drawStaticLayout();
    void updatePlayerFields(bool force = false);

    void drawHeader(
        bool btConnected,
        bool reconnectGrace,
        const String& peerName
    );

    void drawMetadata(
        bool btConnected,
        bool reconnectGrace,
        const String& artist,
        const String& title
    );

    void drawSourceInfo(
        bool btConnected,
        bool btPlaying,
        bool reconnectGrace
    );

    void drawWifiIndicator(
        bool wifiConnected,
        int wifiRssi,
        bool apMode
    );

    void drawClock(
        bool valid,
        const String& clockText
    );

    void drawVolumeIndicator(int volume);

    void showVolumeScreen(
        int volume,
        const String& ip
    );

    void drawVolumeValue(int volume);
    void drawVolumeIp(const String& ip);
    void drawBtTrackNavScreen();

    static int wifiLevel(int rssi);

    static int glyphAdvance(uint32_t codepoint, bool artistFont, uint8_t scale);
    void drawUtf8Line(
        const String& value, int x, int y, int maxWidth,
        uint16_t color, uint16_t background, bool artistFont, uint8_t scale
    );
};
